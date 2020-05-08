#ifndef _MMP_FRAME_H_
#define _MMP_FRAME_H_ 1

#include "bs.h"

enum MPEG_VERSION { VERSION_25, VERSION_RESERVED, VERSION_20, VERSION_10 };
enum MPEG_LAYER { LAYER_RESERVED, LAYER_3, LAYER_2, LAYER_1 };
enum MPEG_MODE { MODE_Stereo, MODE_JointStereo, MODE_DualChannel, MODE_Mono };

struct mpeg_header {
	enum MPEG_VERSION version;
	enum MPEG_LAYER layer;
	unsigned char protection_bit;
	unsigned char bitrate_index;
	unsigned char sampling_frequency;
	unsigned char padding_bit;
	unsigned char private_bit;
	enum MPEG_MODE mode;
	unsigned char mode_extension;
	unsigned char copyright;
	unsigned char original;
	unsigned char emphasis;
};

struct mpeg_frame {
	struct mpeg_header header;

	unsigned short crc16_sum;

	unsigned char lsf;
	unsigned char freeformat;

	unsigned char is_MS;
	unsigned char is_Intensity;

	unsigned nch;

	unsigned bitrate;
	unsigned samplingrate;

	unsigned frame_size;
	unsigned header_size;	// Including CRC-16 data
	unsigned sideinfo_size;
	unsigned maindata_size;

	unsigned pcm_size;
};

int decode_next_frame(struct mpeg_frame* const frame, struct bitstream* const bstream);

#endif // !_MMP_FRAME_H_
