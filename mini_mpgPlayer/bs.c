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

		if (file_name && !(s->file_stream = fopen(file_name, "rb")))
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
		if ((*bstream)->file_stream)
			fclose((*bstream)->file_stream);
		free(*bstream);
		*bstream = NULL;
	}
}

unsigned bs_Length(const struct bs* bstream)
{
	if (bstream)
		return bstream->end_ptr - bstream->bit_buf;
	return 0;
}



int bs_append(struct bitstream* stream, const void* src, int off, int len)
{
	if (len) {
		if (stream->max_ptr - stream->end_ptr < len) {
			if (stream->bit_reservoir != stream->byte_ptr) {
				memcpy(stream->bit_reservoir, stream->byte_ptr, stream->end_ptr - stream->byte_ptr);
				stream->end_ptr -= stream->byte_ptr - stream->bit_reservoir;
				stream->byte_ptr = stream->bit_reservoir;
			}

			if (stream->max_ptr - stream->end_ptr < len)
				len = stream->max_ptr - stream->end_ptr;
		}

		if (len && src) {
			memcpy(stream->end_ptr, (char*)src + off, len);
			stream->end_ptr += len;
		}
	}

	return len;
}

// @@@ byte_pos[0] ___ end_pos ### max_len
int bs_prefect(struct bitstream* stream, int len)
{
	if (len = bs_append(stream, NULL, 0, len)) {
		len = fread(stream->end_ptr, 1, len, stream->file_stream);
		stream->end_ptr += len;
	}

	return len;
}

int bs_skipBytes(struct bitstream* stream, int nBytes)
{
	if (nBytes) {
		if (stream->byte_ptr + nBytes > stream->max_ptr) {
			nBytes = stream->max_ptr - stream->byte_ptr;
		}
		stream->byte_ptr += nBytes;
	}
	stream->bit_pos = 0;
	return nBytes;
}

int bs_skipBits(struct bitstream* stream, int nBits)
{
	stream->bit_pos += nBits;
	stream->byte_ptr += stream->bit_pos >> 3;
	stream->bit_pos &= 7;
}

int bs_backBits(struct bitstream* stream, int nBits)
{
	stream->bit_pos -= nBits;
	stream->byte_ptr += stream->bit_pos >> 3;
	stream->bit_pos &= 7;
}

unsigned bs_readBit(struct bitstream* stream)
{
	unsigned char bit = *stream->byte_ptr << stream->bit_pos;
	bit >>= 7;
	// bit &= 1;
	stream->byte_ptr += ++stream->bit_pos >> 3;
	stream->bit_pos &= 7;
	return bit;
}

// 2 <= nBits <= 24
unsigned bs_readBits(struct bitstream* stream, int nBits)
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



	//int bits = stream->byte_ptr[0];
	//bits <<= 8;
	//bits |= stream->byte_ptr[1];
	//bits <<= 8;
	//bits |= stream->byte_ptr[2];
	//bits <<= 8;
	//bits |= stream->byte_ptr[3];

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

	unsigned bits = stream->byte_ptr[0];
	bits <<= 8;
	bits |= stream->byte_ptr[1];
	bits <<= 8;
	bits |= stream->byte_ptr[2];

	bits <<= stream->bit_pos;
	bits &= 0xffffff;
	bits >>= (24 - nBits);

	stream->byte_ptr += (stream->bit_pos += nBits) >> 3;
	stream->bit_pos &= 7;
	return bits;
}
