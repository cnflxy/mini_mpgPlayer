#ifndef _MMP_AUDIO_H_
#define _MMP_AUDIO_H_ 1

int audio_open(unsigned short nch, unsigned rate);
void audio_close(void);
int play_samples(const void* data, unsigned len);

#endif // !_MMP_AUDIO_H_
