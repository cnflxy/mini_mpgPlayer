#ifndef _MMP_BS_H_
#define _MMP_BS_H_ 1

#include <stdio.h>
#include <stdint.h>

struct bs {
	FILE* file_ptr;

	uint8_t* bit_buf;
	int32_t bit_pos;
	uint8_t* byte_ptr;
	uint8_t* end_ptr;
	const uint8_t* max_ptr;
};

struct bs* bs_Init(uint32_t size, const char* const file_name);
void bs_Release(struct bs** bstream);

uint32_t bs_Avaliable(const struct bs* bstream);
uint32_t bs_Length(const struct bs* bstream);
uint32_t bs_Capacity(const struct bs* bstream);
uint32_t bs_freeSpace(const struct bs* bstream);
uint32_t bs_getBitpos(const struct bs* bstream);

uint32_t bs_Append(struct bs* bstream, const void* src, int32_t off, uint32_t len);
uint32_t bs_Prefect(struct bs* bstream, uint32_t len);

uint32_t bs_skipBytes(struct bs* bstream, uint32_t nBytes);
uint32_t bs_skipBits(struct bs* bstream, uint32_t nBits);
uint32_t bs_backBits(struct bs* bstream, uint32_t nBits);

uint8_t bs_readBit(struct bs* bstream);
// 2 <= nBits <= 17
uint32_t bs_readBits(struct bs* bstream, uint32_t nBits);
uint32_t bs_readByte(struct bs* bstream);
uint32_t bs_readBytes(struct bs* bstream, void* out, uint32_t nBytes);

#endif // !_MMP_BS_H_
