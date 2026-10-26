#ifndef AUDIO_DSP_H
#define AUDIO_DSP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void *synth_create(void);
void synth_destroy(void *handle);
void synth_set_parameter(void *handle, int index, float normalized);
float synth_get_parameter(void *handle, int index);
// Call from the render thread while no synth_render call is in progress.
void synth_all_sounds_off(void *handle);
void synth_set_cc(void *handle, int index, int cc);
void synth_set_channel(void *handle, int channel_one_based);
void synth_set_engine(void *handle, int engine); // 0 dual osc, 1 metallic, 2 four-op FM
// Dual-osc only: last-note-priority mono legato; 0..1 glide maps to 0..2 seconds.
void synth_set_glide(void *handle, int mono_legato, float normalized_time);
void synth_set_page(void *handle, int page); // 0 OSC, 1 FILTER, 2 AMP
int synth_handle_midi(void *handle, uint8_t status, uint8_t data1, uint8_t data2);
void synth_render(void *handle, float *left, float *right, uint32_t frames, double sample_rate);
#ifdef __cplusplus
}
#endif
#endif
