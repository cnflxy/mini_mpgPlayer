#pragma once

//#include <stdint.h>
#include <stdbool.h>

//struct pcm_data {
//	void* buf;
//	uint32_t content_len;
//	uint32_t cur_pos[2];
//	uint32_t size;
//	bool lock;
//};

bool audio_open(unsigned short nch, unsigned rate);
void audio_close(void);
bool play_samples(const void* data, unsigned len);
