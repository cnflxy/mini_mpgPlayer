#pragma once

#include "bitstream.h"
#include <stdbool.h>

enum MPEG_VERSION { VERSION_25, VERSION_RESERVED, VERSION_20, VERSION_10 };
enum MPEG_LAYER { LAYER_RESERVED, LAYER_3, LAYER_2, LAYER_1 };
enum MPEG_MODE { MODE_Stereo, MODE_JointStereo, MODE_DualChannel, MODE_Mono };

struct mpeg_header {
	enum MPEG_VERSION version;
	enum MPEG_LAYER layer;
	unsigned protection_bit;
	unsigned bitrate_index;
	unsigned sampling_frequency;
	unsigned padding_bit;
	unsigned private_bit;
	enum MPEG_MODE mode;
	unsigned mode_extension;
	unsigned copyright;
	unsigned original;
	unsigned emphasis;
};

struct mpeg_frame {
	struct mpeg_header header;

	bool lsf;
	bool is_MS;
	bool is_Intensity;
	bool freeformat;

	unsigned bitrate;
	unsigned samplingrate;

	int nch;

	unsigned frame_size;
	unsigned sideinfo_size;
	unsigned maindata_size;
};

int decode_next_frame(struct mpeg_frame* frame, struct bitstream* stream);
