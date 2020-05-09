#ifndef _MMP_SYNTH_H_
#define _MMP_SYNTH_H_ 1

#include "audio.h"

void init_synthesis_tabs(void);
//void synthesis_subband_filter(const float samples_in[32], unsigned char pcm_out[32 * 2 * 2], unsigned pcm_out_index[2], int ch, int nch);
void synthesis_subband_filter(const float xr[32 * 18], struct pcm_stream *const pcm_out, int ch, int nch);

#endif // !_MMP_SYNTH_H_
