#ifndef _MMP_AUDIO_H_
#define _MMP_AUDIO_H_ 1

struct pcm_stream {
	unsigned char* pcm_buf;
	unsigned read_off;
	unsigned write_off[2]; // {0�� 2}

	unsigned audio_buf_size;	// pcm_size * 4�� �����ݴﵽ���ֵ�����һ�δ���
	unsigned pcm_buf_size;	// audio_buf_size * 4�������ݴﵽ���ֵ����и�λ
};

int audio_open(unsigned rate);
void audio_close(void);
int play_samples(const void* data, unsigned len);

#endif // !_MMP_AUDIO_H_
