#ifndef _MMP_BS_H_
#define _MMP_BS_H_ 1

#include <stdio.h>

struct bs {
	FILE* file_stream;

	unsigned char* bit_buf;
	int bit_pos;
	unsigned char* byte_ptr;
	unsigned char* end_ptr;
	const unsigned char* max_ptr;
};

struct bs* bs_Init(unsigned size, const char* const file_name);
void bs_Release(struct bs** bstream);

unsigned bs_Length(const struct bs* bstream);

int bs_Append(struct bs* bstream, const void* src, int off, int len);
int bs_prefect(struct bs* bstream, int len);

int bs_skipBytes(struct bs* bstream, int nBytes);
int bs_skipBits(struct bs* bstream, int nBits);
int bs_backBits(struct bs* bstream, int nBits);

unsigned bs_readBit(struct bs* bstream);
// 2 <= nBits <= 17
unsigned bs_readBits(struct bs* bstream, int nBits);
int bs_readBytes(struct bs* bstream, void* out, int nBytes);

#endif
