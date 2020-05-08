#include "frame.h"

// __bitrate_table[lsf][layer - 1][bitrate_index]
static const unsigned short _bitrate_table[2][3][15] = {
	{ { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 },		// layer 3
	{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 }, 		// layer 2
	{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 } },	// layer 1

	{ { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 }, 	// layer 3
	{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },		// layer 2
	{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 } }	// layer 1
};

// _samplingrate_table[version][sampling_frequency]
static const unsigned short _samplingrate_table[4][3] = {
	{ 11025, 12000, 8000 },
	{ 0 },
	{ 22050, 24000, 16000 },
	{ 44100, 48000, 32000 }
};

// _layer3_sideinfo_size[lsf][nch - 1]
static const unsigned char _l3_sideinfo_size[2][4] = {
	{ 17, 32 },
	{ 9, 17 }
};

// _samples1frame_table[lsf][layer-1]
static const unsigned short _samples1frame_table[2][3] = {
	{ 384, 1152, 1152 },
	{ 384, 1152, 576 }
};

static void decode_header(struct mpeg_header* const header, const unsigned h)
{
	header->version = (h >> 19) & 3;
	header->layer = (h >> 17) & 3;
	header->protection_bit = (h >> 16) & 1;
	header->bitrate_index = (h >> 12) & 0xf;
	header->sampling_frequency = (h >> 10) & 3;
	header->padding_bit = (h >> 9) & 1;
	header->private_bit = (h >> 8) & 1;
	header->mode = (h >> 6) & 3;
	header->mode_extension = (h >> 4) & 3;
	header->copyright = (h >> 3) & 1;
	header->original = (h >> 2) & 1;
	header->emphasis = h & 3;
}

static unsigned valid_header(const unsigned h)
{
	if ((h & 0xffe00000) != 0xffe00000) {
		int r = 1;
		if ((h & 0xffe000) != 0xffe000) {
			++r;
			if ((h & 0xffe0) != 0xffe0) {
				++r;
				if ((h & 0xff) != 0xff) {
					++r;
				}
			}
		}
		return r;
	}

	if ((h & 0x180000) == 0x80000 || (h & 0x60000) == 0)
		return 2;
	if ((h & 0xf000) == 0xf000)
		return 2;
	if ((h & 0xc00) == 0xc00 || (h & 0x3) == 0x2)
		return 2;

	return 0;
}

static unsigned _frame_count = 0;
static int sync_frame(struct mpeg_header* const header, struct bs* const bstream)
{
	unsigned h = 0, need_read = 4, skipped = 0, tmp;

	while (need_read) {
		if ((tmp = bs_Length(bstream)) < need_read) {
			tmp = need_read + 4 - tmp;
			if (tmp != bs_Prefect(bstream, tmp))
				return -1;
		}

		while (need_read) {
			h <<= 8;
			h |= bs_readByte(bstream);
			--need_read;
		}

		if (need_read = valid_header(h)) {
			skipped += need_read;
			if (skipped >> 20) {
				fprintf(stderr, "[E] already skipped %ubytes\n", skipped);
				return -1;
			}
			continue;
		}

		decode_header(header, h);
	}

	if (skipped) {
		fprintf(stderr, "[W] frame#%u skipped %ubytes\n", _frame_count, skipped);
	}

	++_frame_count;

	return 0;
}

static int get_frame_size(const struct mpeg_frame* const frame)
{
	// _samples1frame_table[header->version][header->layer] * header->bitrate * 1000 / 8 / header->samplingrate + header->padding? (header->layer == MPEG_LAYER_1? 4: 1): 0;

	const struct mpeg_header* const header = &frame->header;

	if (header->layer == LAYER_1)
		return (12 * frame->bitrate * 1000 / frame->samplingrate + header->padding_bit) * 4;
	else if (header->layer == LAYER_2 || (header->version == VERSION_10 && header->layer == LAYER_3))
		return 144 * frame->bitrate * 1000 / frame->samplingrate + header->padding_bit;
	else if (header->version != VERSION_10 && header->layer == LAYER_3)
		return 72 * frame->bitrate * 1000 / frame->samplingrate + header->padding_bit;

	return 0;
}

int decode_next_frame(struct mpeg_frame* const frame, struct bitstream* const bstream)
{
	struct mpeg_header* const header = &frame->header;

	if (sync_frame(header, bstream) == -1) {
		return -1;
	}

	frame->lsf = header->version != VERSION_10;
	frame->freeformat = header->bitrate_index == 0;

	frame->is_MS = header->mode == MODE_JointStereo && (header->mode_extension & 2);
	frame->is_Intensity = header->mode == MODE_JointStereo && (header->mode_extension & 1);

	frame->nch = header->mode == MODE_Mono ? 1 : 2;

	frame->bitrate = _bitrate_table[frame->lsf][header->layer - 1][header->bitrate_index];
	frame->samplingrate = _samplingrate_table[header->version][header->sampling_frequency];

	frame->frame_size = !frame->freeformat ? get_frame_size(frame) : 0;
	frame->sideinfo_size = header->layer == LAYER_3 ? _l3_sideinfo_size[frame->lsf][frame->nch - 1] : 0;

	frame->pcm_size = (frame->lsf ? 2304U : 4608U) >> (frame->nch == 1);

	frame->header_size = 4;
	if (!header->protection_bit)
		frame->header_size += 2;

	if (!frame->freeformat)
		frame->maindata_size = frame->frame_size - frame->header_size - frame->sideinfo_size;
	else frame->maindata_size = 0;

	unsigned need = bs_Length(bstream);
	if (need < frame->frame_size) {
		need = frame->frame_size - need;
		if (need != bs_prefect(bstream, need)) {
			return -1;
		}
	}

	if (!header->protection_bit) {
		frame->crc16_sum = bs_readByte(bstream);
		frame->crc16_sum <<= 8;
		frame->crc16_sum |= bs_readByte(bstream);
	}

	return 0;
}
