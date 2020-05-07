#ifndef _MMP_BS_H_
#define _MMP_BS_H_ 1

#include <stdio.h>

struct bs {
	FILE* file_ptr;

	unsigned char* bit_buf;
	unsigned bit_pos;
	unsigned char* byte_ptr;
	unsigned char* end_ptr;
	const unsigned char* max_ptr;
};

struct bs* bs_Init(unsigned size, const char* const file_name);
void bs_Release(struct bs** bstream);

unsigned bs_Length(const struct bs* bstream);
int bs_getBitpos(const struct bs* bstream);

unsigned bs_Append(struct bs* bstream, const void* src, int off, unsigned len);
unsigned bs_Prefect(struct bs* bstream, unsigned len);

unsigned bs_skipBytes(struct bs* bstream, unsigned nBytes);
unsigned bs_skipBits(struct bs* bstream, unsigned nBits);
unsigned bs_backBits(struct bs* bstream, unsigned nBits);

unsigned char bs_readBit(struct bs* bstream);
// 2 <= nBits <= 17
unsigned bs_readBits(struct bs* bstream, unsigned nBits);
unsigned bs_readByte(struct bs* bstream);
unsigned bs_readBytes(struct bs* bstream, void* out, unsigned nBytes);

#endif // !_MMP_BS_H_
