#include "bs.h"
#include <stdlib.h>
#include <string.h>

struct bs* bs_Init(unsigned size, const char* const file_name)
{
	struct bs* s;

	do {
		s = calloc(1, sizeof(struct bs));
		if (!s) break;

		if (size) {
			if (!(s->bit_buf = malloc(size + 8))) // ÈÝ´í
				break;
			s->byte_ptr = s->end_ptr = s->bit_buf;
			s->max_ptr = s->bit_buf + size;
		}

		if (file_name && !(s->file_ptr = fopen(file_name, "rb")))
			break;

		return s;
	} while (0);

	if (s) {
		if (s->bit_buf)
			free(s->bit_buf);
		free(s);
	}

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

unsigned bs_Length(const struct bs* bstream)
{
	//if (bstream)
	return bstream->end_ptr - bstream->bit_buf;
	//return 0;
}

int bs_getBitpos(const struct bs* bstream)
{
	return bstream->bit_pos;
}

unsigned bs_Append(struct bs* bstream, const void* src, int off, unsigned len)
{
	if (len) {
		if (bstream->max_ptr - bstream->end_ptr < len) {
			unsigned tmp = bs_Length(bstream);
			if (tmp) {
				memcpy(bstream->bit_buf, bstream->byte_ptr, tmp);
				bstream->byte_ptr = bstream->bit_buf;
				bstream->end_ptr = bstream->bit_buf + tmp;
			}

			if (bstream->max_ptr - bstream->end_ptr < len)
				len = bstream->max_ptr - bstream->end_ptr;
		}

		if (src && len) {
			memcpy(bstream->end_ptr, (char*)src + off, len);
			bstream->end_ptr += len;
		}
	}

	return len;
}

// @@@ byte_pos[0] ___ end_pos ### max_len
unsigned bs_Prefect(struct bs* bstream, unsigned len)
{
	if(len = bs_Append(bstream, NULL, 0, len)) {
		len = fread(bstream->end_ptr, 1, len, bstream->file_ptr);
		bstream->end_ptr += len;
	}

	return len;
}

unsigned bs_skipBytes(struct bs* bstream, unsigned nBytes)
{
	if (nBytes) {
		if (bstream->byte_ptr + nBytes > bstream->max_ptr)
			nBytes = bstream->max_ptr - bstream->byte_ptr;
		if (nBytes) {
			bstream->byte_ptr += nBytes;
			bstream->bit_pos = 0;
		}
	}

	return nBytes;
}

unsigned bs_skipBits(struct bs* bstream, unsigned nBits)
{
	bstream->bit_pos += nBits;
	bstream->byte_ptr += bstream->bit_pos >> 3;
	bstream->bit_pos &= 7;

	return nBits;
}

unsigned bs_backBits(struct bs* bstream, unsigned nBits)
{
	bstream->bit_pos -= nBits;
	bstream->byte_ptr -= bstream->bit_pos >> 3;
	bstream->bit_pos &= 7;

	return nBits;
}

unsigned char bs_readBit(struct bs* bstream)
{
	unsigned char bit = *bstream->byte_ptr << bstream->bit_pos;
	bit >>= 7;
	bstream->byte_ptr += ++bstream->bit_pos >> 3;
	bstream->bit_pos &= 7;

	return bit;
}

// 2 <= nBits <= 24
unsigned bs_readBits(struct bs* bstream, unsigned nBits)
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

	unsigned bits = bstream->byte_ptr[0];
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

unsigned bs_readByte(struct bs* bstream)
{
	unsigned byte = *bstream->bit_buf++;
	return byte;
}

unsigned bs_readBytes(struct bs* bstream, void* out, unsigned nBytes)
{
	return 0;
}
