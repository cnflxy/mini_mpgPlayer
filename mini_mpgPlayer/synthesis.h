#pragma once

void init_synthesis_tabs(void);
//int synthesisSubBand(float* samples, int ch, unsigned char* buf, int off, int nch);
void synthesis_subband_filter(const float samples_in[32], unsigned char pcm_out[32 * 2 * 2], unsigned pcm_out_index[2], int ch, int nch);
// void synthesis_subband_filter(const float samples_in[32], char pcm_out[32 * 2 * 2], unsigned pcm_out_index[2], int ch, int nch);
void synthesis_full_filter(const float samples_in[32 * 18], unsigned char pcm_out[32 * 18 * 2 * 2], unsigned pcm_out_index[2], int ch, int nch);
