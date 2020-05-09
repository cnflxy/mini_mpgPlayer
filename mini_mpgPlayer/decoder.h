#ifndef _MMP_DECODER_H_
#define _MMP_DECODER_H_ 1

#include "bs.h"
#include "frame.h"
#include "audio.h"
#include <stdio.h>

#define LOG(_Type, _Func, _Msg) fprintf(stderr, "[%c] %s:%d %s::%s -> %s\n", _Type, __FILE__, __LINE__, __func__, (_Func), (_Msg))
#define LOG_E(_Func, _Msg) LOG('E', _Func, _Msg)
#define LOG_W(_Func, _Msg) LOG('W', _Func, _Msg)
#define LOG_I(_Func, _Msg) LOG('I', _Func, _Msg)

enum OUTPUT_FLAGS { OUTPUT_AUDIO = 0x1, OUTPUT_FILE = 0x2 };

struct decoder_handle {
	struct bs* file_stream;
	struct bs* sideinfo_stream;
	struct bs* maindata_stream;

	struct mpeg_frame cur_frame;

	int output_flags;
	struct pcm_stream pcm;
	FILE* wav_ptr;
};

struct decoder_handle* decoder_Init(const char* const mp3_file_name, const int output_flags, const char* const wav_file_name);
void decoder_Release(struct decoder_handle** const handle);
unsigned decoder_Run(struct decoder_handle* const handle);

#endif // !_MMP_DECODER_H_
