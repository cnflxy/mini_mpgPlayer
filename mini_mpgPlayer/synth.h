#ifndef _MMP_SYNTH_H_
#define _MMP_SYNTH_H_ 1

#include "audio.h"

void init_synthesis_tabs(void);
//void synthesis_subband_filter(const float samples_in[32], unsigned char pcm_out[32 * 2 * 2], unsigned pcm_out_index[2], int ch, int nch);
void synthesis_subband_filter(const float s[32], const uint8_t ch, const uint8_t nch, uint8_t* pcm_buf, uint32_t* off);

#endif // !_MMP_SYNTH_H_
