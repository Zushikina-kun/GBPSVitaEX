/* GBAVitaEX — src/audio/audio_output.h
 * Thin wrapper around sceAudioOut for the GBA engine's audio path.
 * (GB/GBC uses its own dedicated thread in gb_engine.c)
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void audio_output_init(int sample_rate, int samples_per_frame);
void audio_output_shutdown(void);
/* Submit one frame of stereo PCM-16 audio. */
void audio_output_submit(const int16_t *buf, int frames);
void audio_output_set_volume(int vol_0_to_100);
