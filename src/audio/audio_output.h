/* GBVitaEX — src/audio/audio_output.h
 * GBA audio output: thin sceAudioOut wrapper with optional TDHS pitch
 * correction for fast-forward mode.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void audio_output_init(int sample_rate, int samples_per_frame);
void audio_output_shutdown(void);

/* Submit stereo PCM-16 audio.
 * When pitch_correct=true the samples are time-stretched to remove the
 * speed-up pitch shift caused by fast-forward — music stays at normal pitch
 * while playing faster.  ratio = ff_speed_pct / 100.0f (e.g. 2.0 for 2×). */
void audio_output_submit_ff(const int16_t *buf, int frames,
                            bool pitch_correct, float ratio);

/* Convenience wrapper — no pitch correction (normal speed) */
static inline void audio_output_submit(const int16_t *buf, int frames) {
    audio_output_submit_ff(buf, frames, false, 1.0f);
}

void audio_output_set_volume(int vol_0_to_100);
