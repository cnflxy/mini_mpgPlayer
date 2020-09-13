#ifndef _MMP_AUDIO_H_
#define _MMP_AUDIO_H_ 1

#include <stdint.h>

struct pcm_stream {
	uint8_t* pcm_buf;
	uint32_t read_off;
	uint32_t write_off[2]; // {0， 2}

	uint32_t audio_buf_size;	// pcm_size * 4， 当内容达到这个值便进行一次处理
	uint32_t pcm_buf_size;	// audio_buf_size * 4，当内容达到这个值便进行复位
};

int audio_open(uint32_t rate);
void audio_close(void);
int play_samples(const void* data, uint32_t len);

#endif // !_MMP_AUDIO_H_
