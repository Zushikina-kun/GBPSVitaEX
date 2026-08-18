/* GBAVitaEX — src/audio/audio_output.c
 * Thin sceAudioOut wrapper used by the GBA engine path.
 * Runs synchronously in the main thread (gpSP renders audio per-frame).
 */

#include "audio_output.h"
#include <psp2/audioout.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_BATCH 1024  /* sceAudioOut limit per call */

static int   s_port      = -1;
static int   s_spf       = 0;   /* samples per frame */
static float s_vol_scale = 1.0f;

void audio_output_init(int sample_rate, int samples_per_frame) {
    if (s_port >= 0) return;
    /* Clamp to hardware limits */
    if (samples_per_frame > MAX_BATCH) samples_per_frame = MAX_BATCH;

    s_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,
                                  samples_per_frame, sample_rate,
                                  SCE_AUDIO_OUT_MODE_STEREO);
    if (s_port < 0)
        fprintf(stderr, "[audio_output] sceAudioOutOpenPort failed: 0x%08X\n", s_port);

    s_spf = samples_per_frame;
}

void audio_output_shutdown(void) {
    if (s_port >= 0) {
        sceAudioOutReleasePort(s_port);
        s_port = -1;
    }
}

void audio_output_submit(const int16_t *buf, int frames) {
    if (s_port < 0 || !buf) return;
    int done = 0;
    while (done < frames) {
        int batch = frames - done;
        if (batch > MAX_BATCH) batch = MAX_BATCH;
        sceAudioOutOutput(s_port, buf + done * 2);
        done += batch;
    }
}

void audio_output_set_volume(int vol_0_to_100) {
    if (vol_0_to_100 < 0)   vol_0_to_100 = 0;
    if (vol_0_to_100 > 100) vol_0_to_100 = 100;
    s_vol_scale = vol_0_to_100 / 100.0f;
    if (s_port >= 0) {
        int v = (int)(SCE_AUDIO_OUT_MAX_VOL * s_vol_scale);
        sceAudioOutSetVolume(s_port,
            SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO, &v);
    }
}
