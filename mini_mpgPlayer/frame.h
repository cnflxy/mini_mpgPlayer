#ifndef _MMP_FRAME_H_
#define _MMP_FRAME_H_ 1

#include "bs.h"
#include <stdbool.h>

enum MPEG_VERSION { VERSION_25, VERSION_RESERVED, VERSION_20, VERSION_10 };
enum MPEG_LAYER { LAYER_RESERVED, LAYER_3, LAYER_2, LAYER_1 };
enum MPEG_MODE { MODE_Stereo, MODE_JointStereo, MODE_DualChannel, MODE_Mono };

struct mpeg_header {
	enum MPEG_VERSION version;
	enum MPEG_LAYER layer;
	uint8_t protection_bit;
	uint8_t bitrate_index;
	uint8_t sampling_frequency;
	uint8_t padding_bit;
	uint8_t private_bit;
	enum MPEG_MODE mode;
	uint8_t mode_extension;
	uint8_t copyright;
	uint8_t original;
	uint8_t emphasis;
};

struct mpeg_frame {
	struct mpeg_header header;
	uint16_t crc16_sum;

	bool is_lsf;
	bool is_freeformat;
	bool is_MS;
	bool is_Intensity;
	uint8_t nch;
	uint32_t bitrate;
	uint32_t samplingrate;

	uint32_t frame_size;
	uint32_t header_size;	// Including CRC-16 data
	uint32_t sideinfo_size;
	uint32_t maindata_size;

	uint32_t pcm_size;
};

int decode_next_frame(struct mpeg_frame* const frame, struct bs* const bstream);

#endif // !_MMP_FRAME_H_
