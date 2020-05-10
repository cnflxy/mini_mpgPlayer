#ifndef _MMP_LAYER3_H_
#define _MMP_LAYER3_H_ 1

#include "decoder.h"

/*
* IMDCT (时域 -> 频率)
* 长块的块长为 18 个样本，短块的块长为 6 个样本。
* 在短块模式下，3 个短块代替 1 个 长块 ， 而短块的大小恰好是一个长块的 1/3，所以 IMDCT 的样本数不受块长的影响
* 在混合块模式下，IMDCT 对最低频的 2 个子带使用长块，而对其余的 30 个子带使用短块
* 短块也包含 18 个数据，但是是由 6 个数据独立加窗后在经过连接计算得到的
*
* 
*
*/

struct ch_info {
	unsigned short part2_3_len;			// 12 bits, 主数据位数, part2_len(scalefactor, 缩放因子位数) + part3_len(Huffman code, 哈夫曼编码区位数)
	unsigned short big_values;			// 9 bits
	unsigned short global_gain;			// 8 bits
	unsigned short scalefac_compress;	// 4 bits

	unsigned char win_switch_flag;	// 1 bit
	unsigned char block_type;		// 2 bits, win_switch_flag == 1(1/3: 混合块(1: 开始块, 3: 结束块), 2: 短块), win_switch_flag == 0(0: 长块)
	unsigned char mixed_block_flag;	// 1 bit, win_switch_flag == 1
	unsigned char table_select[3];	// 2*5bits(win_switch_flag == 1), 3*5bits(win_switch_flag == 0), table_select[region]
	unsigned char subblock_gain[3];	// 3*3bits, win_switch_flag == 1, subblock_gain[window]
	unsigned char region0_count;	// 4 bits, win_switch_flag == 0
	unsigned char region1_count;	// 3 bits, win_switch_flag == 0

	unsigned char preflag;				// 1 bit
	unsigned char scalefac_scale;		// 1 bit
	unsigned char count1table_select;	// 1 bit

	unsigned short part2_len;	// scalefactor len
	unsigned nonzero_len;	// Huffman区（大值区和小值区）
};

struct gr_info {
	struct ch_info ch[2];
};

struct l3_sideinfo {
	unsigned short main_data_begin;	// 9 bits, 指出主数据在同步字之前多少个字节开始
	unsigned char private_bits;		// mono: 5 bits, dual: 3 bits

	unsigned char scfsi[2][4];	// 4*1 bits, scfsi[ch][scfsi_band], scale-factor selector information[ band]
	struct gr_info gr[2];
};

void l3_init(const struct mpeg_header* const header);
int l3_decode_samples(struct decoder_handle* const handle, unsigned frame_count);

#endif // !_MMP_LAYER3_H_
