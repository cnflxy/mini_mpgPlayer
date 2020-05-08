#ifndef _MMP_LAYER3_H_
#define _MMP_LAYER3_H_ 1

#include "decoder.h"

struct ch_info {
	unsigned short part2_3_len;	// 12 bits	// part2_len + huffbits_len
	unsigned short big_values;	// 9 bits
	unsigned short global_gain;	// 8 bits
	unsigned short scalefac_compress;	// 4 bits

	unsigned char win_switch_flag;	// 1 bit
	unsigned char block_type;	// 2 bits
	unsigned char mixed_block_flag;	// 1 bit
	unsigned char table_select[3];	// 5 bits
	unsigned char subblock_gain[3];	// 3 bits
	unsigned char region0_count;	// 4 bits
	unsigned char region1_count;	// 3 bits

	unsigned char preflag;	// 1 bit
	unsigned char scalefac_scale;	// 1 bit
	unsigned char count1table_select;	// 1 bit

	unsigned char scalefac_l[21];
	unsigned char scalefac_s[12 * 3];

	//int part2_len;	// scalefactor len
	int nonzero_len;	// Huffman区（大值区和小值区）
};

struct l3_sideinfo {
	unsigned int main_data_begin;	// 9 bits, 指出当前帧最后1bit的位置
	unsigned char private_bits;	// mono: 5 bits, dual: 3 bits
	unsigned char scfsi[2];	// 4*1 bits	// scale-factor selector information

	struct ch_info info_ch[2][2]; // info_ch[ch][gr]
};

void l3_init(const struct mpeg_header* const header);
int l3_decode_samples(struct decoder_handle* const handle, unsigned frame_count);

#endif // !_MMP_LAYER3_H_
