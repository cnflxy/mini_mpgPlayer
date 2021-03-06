#define _CRT_SECURE_NO_WARNINGS

#include "bs.h"
#include <stdlib.h>
#include <string.h>

struct bs* bs_Init(uint32_t size, const char* const file_name)
{
	struct bs* s;

	do {
		s = calloc(1, sizeof(struct bs));
		if (!s) break;

		if (size) {
			if (!(s->bit_buf = malloc(size + 8))) // �ݴ�
				break;
			s->byte_ptr = s->end_ptr = s->bit_buf;
			s->max_ptr = s->bit_buf + size;
		}

		if (file_name && !(s->file_ptr = fopen(file_name, "rb")))
			break;

		return s;
	} while (0);

	bs_Release(&s);

	return NULL;
}

void bs_Release(struct bs** bstream)
{
	if (bstream && *bstream) {
		if ((*bstream)->bit_buf)
			free((*bstream)->bit_buf);
		if ((*bstream)->file_ptr)
			fclose((*bstream)->file_ptr);
		free(*bstream);
		*bstream = NULL;
	}
}

uint32_t bs_Avaliable(const struct bs* bstream)
{
	return bstream->end_ptr - bstream->byte_ptr;
}

uint32_t bs_Length(const struct bs* bstream)
{
	//if (bstream)
	return bstream->end_ptr - bstream->bit_buf;
	//return 0;
}

uint32_t bs_Capacity(const struct bs* bstream)
{
	return bstream->max_ptr - bstream->bit_buf;
}

uint32_t bs_freeSpace(const struct bs* bstream)
{
	return bstream->max_ptr - bstream->end_ptr;
}

uint32_t bs_getBitpos(const struct bs* bstream)
{
	return bstream->bit_pos;
}

// bit_buf --> byte_ptr --> end_ptr --> max_ptr
// 

uint32_t bs_Append(struct bs* bstream, const void* src, int32_t off, uint32_t len)
{
	if (len) {
		//if (len > bs_Capacity(bstream))
		//	len = bs_Capacity(bstream);

		//if (len > bs_freeSpace(bstream)) {
		//	unsigned tmp = bstream->end_ptr - bstream->byte_ptr;
		//	unsigned need = len - bs_freeSpace(bstream);
		//	if (need * 2 >= bs_Length(bstream)) {
		//		memcpy(bstream->end_ptr - need * 2, bstream->end_ptr - need, need);
		//		bstream->end_ptr -= need;
		//		bstream->byte_ptr = bstream->end_ptr - tmp;
		//	} else {
		//		memcpy(bstream->bit_buf, bstream->end_ptr - need, need);
		//		bstream->end_ptr = bstream->bit_buf + need;
		//		bstream->byte_ptr = bstream->end_ptr - tmp;
		//	}

		//	if (len > bs_freeSpace(bstream))
		//		len = bs_freeSpace(bstream);
		//}

		if (len > bs_freeSpace(bstream)) {
			memcpy(bstream->bit_buf, bstream->byte_ptr, bs_Avaliable(bstream));
			bstream->end_ptr = bstream->bit_buf + bs_Avaliable(bstream);
			bstream->byte_ptr = bstream->bit_buf;
			// bstream->bit_pos = 0;

			if (len > bs_freeSpace(bstream))
				len = bs_freeSpace(bstream);
		}

		if (src && len) {
			memcpy(bstream->end_ptr, (char*)src + off, len);
			bstream->end_ptr += len;
		}
	}

	return len;
}

// @@@ byte_pos[0] ___ end_pos ### max_len
uint32_t bs_Prefect(struct bs* bstream, uint32_t len)
{
	if (len == bs_Append(bstream, NULL, 0, len)) {
		len = fread(bstream->end_ptr, 1, len, bstream->file_ptr);
		bstream->end_ptr += len;
	}

	return len;
}

uint32_t bs_skipBytes(struct bs* bstream, uint32_t nBytes)
{
	if (nBytes > bs_Avaliable(bstream) + bs_freeSpace(bstream))
		nBytes = bs_Avaliable(bstream) + bs_freeSpace(bstream);

	bstream->byte_ptr += nBytes;
	bstream->bit_pos = 0;

	return nBytes;
}

uint32_t bs_skipBits(struct bs* bstream, uint32_t nBits)
{
	bstream->bit_pos += nBits;
	bstream->byte_ptr += bstream->bit_pos >> 3;
	bstream->bit_pos &= 7;

	return nBits;
}

uint32_t bs_backBits(struct bs* bstream, uint32_t nBits)
{
	bstream->bit_pos -= nBits;
	bstream->byte_ptr += bstream->bit_pos >> 3;
	bstream->bit_pos &= 7;

	return nBits;
}

uint8_t bs_readBit(struct bs* bstream)
{
	uint8_t bit = *bstream->byte_ptr << bstream->bit_pos;
	bit >>= 7;
	bstream->byte_ptr += ++bstream->bit_pos >> 3;
	bstream->bit_pos &= 7;

	return bit;
}

// 2 <= nBits <= 24
uint32_t bs_readBits(struct bs* bstream, uint32_t nBits)
{
	//int bits = 0, i = 0, n = (nBits + stream->bit_pos + 7) >> 3;
	//while (i < n) {
	//	bits <<= 8;
	//	bits = stream->bit_reservoir[stream->byte_pos + i++];
	//}

	//bits <<= stream->bit_pos;
	//bits &= 0xffffffff >> (32 - nBits);
	// bits >>= (n << 3) - nBits;

	//bits >>= 8 - ((nBits & 7) + stream->bit_pos);
	////bits &= 0xffffffff >> (32 - nBits);
	//bits &= ~(0xffffffff << nBits);

	//int bits = stream->bit_reservoir[stream->byte_pos], i = 0, n = (nBits + stream->bit_pos) >> 3;
	//while (i < n) {
	//	bits <<= 8;
	//	bits |= stream->bit_reservoir[stream->byte_pos + ++i];
	//}

	////bits <<= stream->bit_pos;
	////bits >>= 8 - (nBits & 7);
	//bits >>= 8 - ((nBits & 7) + stream->bit_pos);
	//bits &= ~(0xffffffff << nBits);

	//unsigned bits = *(unsigned*)stream->byte_ptr;

	//__asm {
	//	//push eax
	//	mov eax, dword ptr[bits]
	//	bswap eax
	//	mov dword ptr[bits], eax
	//	//pop eax
	//}

	//	bits <<= stream->bit_pos;
	//bits >>= (32 - nBits);
	//bits &= ~(0xffffffffU << nBits);

	uint32_t bits = bstream->byte_ptr[0];
	bits <<= 8;
	bits |= bstream->byte_ptr[1];
	bits <<= 8;
	bits |= bstream->byte_ptr[2];

	bits <<= bstream->bit_pos;
	bits &= 0xffffff;
	bits >>= (24 - nBits);

	bstream->byte_ptr += (bstream->bit_pos += nBits) >> 3;
	bstream->bit_pos &= 7;

	return bits;
}

uint32_t bs_readByte(struct bs* bstream)
{
	unsigned byte = *bstream->byte_ptr++;
	return byte;
}

uint32_t bs_readBytes(struct bs* bstream, void* out, uint32_t nBytes)
{
	return 0;
}
