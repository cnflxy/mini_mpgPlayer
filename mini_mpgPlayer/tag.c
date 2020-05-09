#include "tag.h"
#include <ctype.h>

static void safe_print(const char* str, int len)
{
	while (len-- > 0)
		putchar(*str++);
	putchar('\n');
}

void decode_id3v1(struct bs* const bstream)
{
	if (fseek(bstream->file_ptr, -128, SEEK_END))
		return;

	if (bs_Prefect(bstream, 128) == 128) {
		if (bstream->byte_ptr[0] == 'T' && bstream->byte_ptr[1] == 'A' && bstream->byte_ptr[2] == 'G') {
			printf("ID3 1.%d\n", bstream->byte_ptr[126] && bstream->byte_ptr[126] != ' ');
			if (bstream->byte_ptr[3] && bstream->byte_ptr[3] != ' ') {
				printf("title: ");
				safe_print(bstream->byte_ptr + 3, 30);
			}
			if (bstream->byte_ptr[33] && bstream->byte_ptr[33] != ' ') {
				printf("artist: ");
				safe_print(bstream->byte_ptr + 33, 30);
			}
			if (bstream->byte_ptr[63] && bstream->byte_ptr[63] != ' ') {
				printf("album: ");
				safe_print(bstream->byte_ptr + 63, 30);
			}
			if (bstream->byte_ptr[97] && bstream->byte_ptr[97] != ' ') {
				printf("comment: ");
				safe_print(bstream->byte_ptr + 97, 29);
			}
			if (bstream->byte_ptr[93] && bstream->byte_ptr[93] != ' ') {
				printf("year: %d%d%d%d\n", bstream->byte_ptr[93], bstream->byte_ptr[94], bstream->byte_ptr[95], bstream->byte_ptr[96]);
			}
			if (bstream->byte_ptr[126] && bstream->byte_ptr[126] != ' ')
				printf("track: %d\n", bstream->byte_ptr[126]);
			printf("genre: %d\n", bstream->byte_ptr[127]);
			putchar('\n');
		}
	}

	fseek(bstream->file_ptr, 0, SEEK_SET);
	bstream->end_ptr = bstream->bit_buf;
}

int decode_id3v2(struct bs* const bstream, unsigned* const size)
{
	if (bs_Prefect(bstream, 10) != 10)
		return -1;

	if (bstream->byte_ptr[0] != 'I' || bstream->byte_ptr[1] != 'D' || bstream->byte_ptr[2] != '3')
		return -1;

	if (bstream->byte_ptr[6] >> 7 || bstream->byte_ptr[7] >> 7 || bstream->byte_ptr[8] >> 7 || bstream->byte_ptr[9] >> 7)
		return -1;

	*size = bstream->byte_ptr[6];
	*size <<= 7;
	*size |= bstream->byte_ptr[7];
	*size <<= 7;
	*size |= bstream->byte_ptr[8];
	*size <<= 7;
	*size |= bstream->byte_ptr[9];

	printf("ID3 2.%d%d\n" \
		"flag: 0x%x\n" \
		"size: %ubytes\n\n",
		bstream->byte_ptr[3], bstream->byte_ptr[4], bstream->byte_ptr[5], *size + 10);

	return 0;
}

#define VBR_TAG_INFO	0x6f666e49U	// CBR (Constant Bit Rate).
#define VBR_TAG_XING	0x676e6958U	// VBR/ABR (Variable Bit Rate/Average Bit Rate).

static int byte2int(const struct bs* const stream, int off)
{
	int i = stream->byte_ptr[off];
	i <<= 8;
	i |= stream->byte_ptr[off + 1];
	i <<= 8;
	i |= stream->byte_ptr[off + 2];
	i <<= 8;
	i |= stream->byte_ptr[off + 3];
	return i;
}

int get_vbr_tag(const struct bs* const bstream, const struct mpeg_frame* const frame)
{
	if (frame->header.version != VERSION_10 || frame->header.layer != LAYER_3)
		return -1;

	unsigned off = frame->sideinfo_size;
	unsigned tag_magic = *(unsigned*)(bstream->byte_ptr + off);

	if (tag_magic == VBR_TAG_INFO)
		puts("Info - CBR (Constant Bit Rate)");
	else if (tag_magic == VBR_TAG_XING)
		puts("Xing - VBR/ABR (Variable Bit Rate/Average Bit Rate)");
	else return -1;
	off += 4;

	unsigned char flags = bstream->byte_ptr[off + 3];
	off += 4;

	double duration = 0;
	if (flags & 0x1) {	// Number of Frames
		int frames = byte2int(bstream, off);
		duration = frames * (frame->lsf ? 576.0 : 1152.0) / frame->samplingrate;
		printf("Number of Frames: %d --> %.2lfsecs\n", frames, duration);
		off += 4;
	}

	if (flags & 0x2) { // Size in Bytes
		int size = byte2int(bstream, off);
		duration = size / (16.0 * frame->samplingrate * frame->nch / 8);
		printf("Size in Bytes: %dbytes --> %.2lfsecs\n", size, duration);
		off += 4;
	}

	if (flags & 0x4) { // TOC Data
		puts("TOC Data: Existent");
		off += 100;
	}

	if (flags & 0x8) { // VBR Scale
		printf("VBR Scale: %d\n", byte2int(bstream, off));
		off += 4;
	}

	char encoder_str[16 + 1];
	unsigned len = 0;
	while (len < 16 && (isalnum(bstream->byte_ptr[off + len]) || bstream->byte_ptr[off + len] == '.' || bstream->byte_ptr[off + len] == ' ')) {
		encoder_str[len] = bstream->byte_ptr[off + len];
		++len;
	}
	if (len >= 4) {
		encoder_str[len] = 0;
		printf("encoder: %s\n", encoder_str);
	}

	//tag_magic = *(unsigned*)(stream->byte_ptr + off);
	//if (tag_magic == 'EMAL') {	// LAME
	//	// LAME<major>.<minor><release>
	//	printf("encoder: LAME %c.%c%c %c\n", stream->byte_ptr[4], stream->byte_ptr[6], stream->byte_ptr[7], stream->byte_ptr[8]);
	//} else if (tag_magic == 'fvaL') {	// Lavf
	//	printf("encoder: Lavf %c%c.%c.%c%c%c\n", stream->byte_ptr[4], stream->byte_ptr[5], stream->byte_ptr[7], stream->byte_ptr[9], stream->byte_ptr[10], stream->byte_ptr[11]);
	//} else if (tag_magic == 'cvaL') {	// Lavc
	//	printf("encoder: Lavc %c%c.%c%c\n", stream->byte_ptr[4], stream->byte_ptr[5], stream->byte_ptr[7], stream->byte_ptr[8]);
	//}
	putchar('\n');
	return 0;
}