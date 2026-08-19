/* GBVitaEX — src/gb/gb_link.c
 * Single-device GB/GBC link cable via mGBA GBSIOLockstep.
 * Two mGBA cores run in the same process, sharing the lockstep controller.
 */

#include "gb_link.h"
#include "gb_engine.h"
#include "gbvitaex.h"

/* mGBA headers */
#include <mgba-util/common.h>
#include <mgba/core/core.h>
#include <mgba/gb/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/sio/lockstep.h>
#include <mgba-util/vfs.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
   Player 2 core state
   (Player 1 is owned by gb_engine.c — we just attach the lockstep to it)
   ────────────────────────────────────────────────────────────────────────── */
#define GB_TEX_STRIDE 256

static struct mCore         *s_p2_core   = NULL;
static struct VFile         *s_p2_rom_vf  = NULL;
static struct VFile         *s_p2_save_vf = NULL;
static uint16_t              s_p2_pixels[GB_SCREEN_H * GB_TEX_STRIDE];
static bool                  s_active     = false;

/* ──────────────────────────────────────────────────────────────────────────
   Lockstep controller
   ────────────────────────────────────────────────────────────────────────── */
static struct GBSIOLockstep     s_lockstep;
static struct GBSIOLockstepNode s_node_p1;
static struct GBSIOLockstepNode s_node_p2;

/* The simplest possible lockstep implementation:
 * lock/unlock/signal/wait are all no-ops because both cores run
 * synchronously on the same thread.  addCycles/useCycles track the
 * inter-core cycle budget. */

static void   ls_lock(struct mLockstep *ls)              { (void)ls; }
static void   ls_unlock(struct mLockstep *ls)            { (void)ls; }
static bool   ls_signal(struct mLockstep *ls, unsigned m){ (void)ls;(void)m; return true; }
static bool   ls_wait(struct mLockstep *ls, unsigned m)  { (void)ls;(void)m; return true; }

static void ls_addCycles(struct mLockstep *ls, int id, int32_t cyc) {
    (void)ls; (void)id; (void)cyc;
}
static int32_t ls_useCycles(struct mLockstep *ls, int id, int32_t cyc) {
    (void)ls; (void)id; return cyc;
}
static int32_t ls_unusedCycles(struct mLockstep *ls, int id) {
    (void)ls; (void)id; return 0;
}
static void ls_unload(struct mLockstep *ls, int id) {
    (void)ls; (void)id;
}

/* ──────────────────────────────────────────────────────────────────────────
   P2 audio — simple ring drain (no dedicated thread; drained on request)
   ────────────────────────────────────────────────────────────────────────── */
#define P2_AUDIO_BUF 2048
static struct mAudioBuffer s_p2_audio_buf;

/* ──────────────────────────────────────────────────────────────────────────
   Public API
   ────────────────────────────────────────────────────────────────────────── */

bool gb_link_start(const char *rom1, const char *save1,
                   const char *rom2, const char *save2) {
    if (s_active) gb_link_stop();

    /* ── Wire the lockstep controller ── */
    GBSIOLockstepInit(&s_lockstep);
    mLockstepInit(&s_lockstep.d);
    s_lockstep.d.lock         = ls_lock;
    s_lockstep.d.unlock       = ls_unlock;
    s_lockstep.d.signal       = ls_signal;
    s_lockstep.d.wait         = ls_wait;
    s_lockstep.d.addCycles    = ls_addCycles;
    s_lockstep.d.useCycles    = ls_useCycles;
    s_lockstep.d.unusedCycles = ls_unusedCycles;
    s_lockstep.d.unload       = ls_unload;

    /* ── Attach P1 (existing gb_engine core) ── */
    struct mCore *p1_core = gb_engine_get_core();
    if (!p1_core) {
        /* P1 not loaded yet; load it */
        if (!gb_engine_load(rom1, save1, false)) {
            fprintf(stderr, "[gb_link] Failed to load P1 ROM\n");
            return false;
        }
        p1_core = gb_engine_get_core();
    }

    GBSIOLockstepNodeCreate(&s_node_p1);
    struct GB *gb1 = p1_core->board;
    gb1->sio.driver = NULL;  /* detach any existing driver */
    if (!GBSIOLockstepAttachNode(&s_lockstep, &s_node_p1)) {
        fprintf(stderr, "[gb_link] Failed to attach P1 node\n");
        return false;
    }
    gb1->sio.driver = &s_node_p1.d;
    s_node_p1.d.init(&s_node_p1.d);

    /* ── Create and init P2 core ── */
    s_p2_core = GBCoreCreate();
    if (!s_p2_core || !s_p2_core->init(s_p2_core)) {
        fprintf(stderr, "[gb_link] Failed to create P2 core\n");
        return false;
    }
    s_p2_core->setVideoBuffer(s_p2_core, (mColor *)s_p2_pixels, GB_TEX_STRIDE);
    s_p2_core->setAudioBufferSize(s_p2_core, P2_AUDIO_BUF);
    mAudioBufferInit(&s_p2_audio_buf, P2_AUDIO_BUF * 4, 2);

    s_p2_rom_vf = VFileOpen(rom2, O_RDONLY);
    if (!s_p2_rom_vf || !s_p2_core->loadROM(s_p2_core, s_p2_rom_vf)) {
        fprintf(stderr, "[gb_link] Failed to load P2 ROM: %s\n", rom2);
        s_p2_core->deinit(s_p2_core); s_p2_core = NULL;
        return false;
    }
    s_p2_save_vf = VFileOpen(save2, O_RDWR | O_CREAT);
    if (s_p2_save_vf) s_p2_core->loadSave(s_p2_core, s_p2_save_vf);

    GBSIOLockstepNodeCreate(&s_node_p2);
    struct GB *gb2 = s_p2_core->board;
    if (!GBSIOLockstepAttachNode(&s_lockstep, &s_node_p2)) {
        fprintf(stderr, "[gb_link] Failed to attach P2 node\n");
        s_p2_core->deinit(s_p2_core); s_p2_core = NULL;
        return false;
    }
    gb2->sio.driver = &s_node_p2.d;
    s_node_p2.d.init(&s_node_p2.d);

    s_p2_core->reset(s_p2_core);

    s_active = true;
    return true;
}

void gb_link_run_frame(void) {
    if (!s_active || !s_p2_core) return;
    /* Both cores are run by gb_engine_run_frame() (P1) and here (P2).
     * The lockstep driver synchronises byte transfers automatically
     * during runFrame via the timing events in GBSIOLockstep. */
    s_p2_core->runFrame(s_p2_core);
}

void gb_link_stop(void) {
    if (!s_active) return;

    /* Detach both SIO drivers before deiniting */
    if (gb_engine_get_core()) {
        struct GB *gb1 = gb_engine_get_core()->board;
        if (gb1->sio.driver) { gb1->sio.driver->deinit(gb1->sio.driver); gb1->sio.driver = NULL; }
        GBSIOLockstepDetachNode(&s_lockstep, &s_node_p1);
    }

    if (s_p2_core) {
        struct GB *gb2 = s_p2_core->board;
        if (gb2->sio.driver) { gb2->sio.driver->deinit(gb2->sio.driver); gb2->sio.driver = NULL; }
        GBSIOLockstepDetachNode(&s_lockstep, &s_node_p2);
        if (s_p2_save_vf) { s_p2_save_vf->close(s_p2_save_vf); s_p2_save_vf = NULL; }
        s_p2_core->unloadROM(s_p2_core);
        s_p2_core->deinit(s_p2_core);
        s_p2_core = NULL;
    }
    if (s_p2_rom_vf) { s_p2_rom_vf->close(s_p2_rom_vf); s_p2_rom_vf = NULL; }

    mLockstepDeinit(&s_lockstep.d);
    mAudioBufferDeinit(&s_p2_audio_buf);
    s_active = false;
}

bool gb_link_active(void)                              { return s_active; }
const uint16_t *gb_link_get_p2_framebuffer(void)      { return s_p2_pixels; }

int gb_link_drain_p2_audio(int16_t *buf, int max_frames) {
    if (!s_active || !s_p2_core) return 0;
    struct mAudioBuffer *ab = s_p2_core->getAudioBuffer(s_p2_core);
    int avail = (int)mAudioBufferAvailable(ab);
    int count = avail < max_frames ? avail : max_frames;
    if (count > 0) mAudioBufferRead(ab, buf, count);
    return count;
}
