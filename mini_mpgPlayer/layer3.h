#ifndef _MMP_LAYER3_H_
#define _MMP_LAYER3_H_ 1

#include "decoder.h"

/*
* IMDCT (ʱ�� -> Ƶ��)
* ����Ŀ鳤Ϊ 18 ���������̿�Ŀ鳤Ϊ 6 ��������
* �ڶ̿�ģʽ�£�3 ���̿���� 1 �� ���� �� ���̿�Ĵ�Сǡ����һ������� 1/3������ IMDCT �����������ܿ鳤��Ӱ��
* �ڻ�Ͽ�ģʽ�£�IMDCT �����Ƶ�� 2 ���Ӵ�ʹ�ó��飬��������� 30 ���Ӵ�ʹ�ö̿�
* �̿�Ҳ���� 18 �����ݣ��������� 6 �����ݶ����Ӵ����ھ������Ӽ���õ���
*
* 
*
*/

struct ch_info {
	unsigned short part2_3_len;			// 12 bits, ������λ��, part2_len(scalefactor, ��������λ��) + part3_len(Huffman code, ������������λ��)
	unsigned short big_values;			// 9 bits
	unsigned short global_gain;			// 8 bits
	unsigned short scalefac_compress;	// 4 bits

	unsigned char win_switch_flag;	// 1 bit
	unsigned char block_type;		// 2 bits, win_switch_flag == 1(1/3: ��Ͽ�(1: ��ʼ��, 3: ������), 2: �̿�), win_switch_flag == 0(0: ����)
	unsigned char mixed_block_flag;	// 1 bit, win_switch_flag == 1
	unsigned char table_select[3];	// 2*5bits(win_switch_flag == 1), 3*5bits(win_switch_flag == 0), table_select[region]
	unsigned char subblock_gain[3];	// 3*3bits, win_switch_flag == 1, subblock_gain[window]
	unsigned char region0_count;	// 4 bits, win_switch_flag == 0
	unsigned char region1_count;	// 3 bits, win_switch_flag == 0

	unsigned char preflag;				// 1 bit
	unsigned char scalefac_scale;		// 1 bit
	unsigned char count1table_select;	// 1 bit

	unsigned short part2_len;	// scalefactor len
	unsigned nonzero_len;	// Huffman������ֵ����Сֵ����
};

struct gr_info {
	struct ch_info ch[2];
};

struct l3_sideinfo {
	unsigned short main_data_begin;	// 9 bits, ָ����������ͬ����֮ǰ���ٸ��ֽڿ�ʼ
	unsigned char private_bits;		// mono: 5 bits, dual: 3 bits

	unsigned char scfsi[2][4];	// 4*1 bits, scfsi[ch][scfsi_band], scale-factor selector information[ band]
	struct gr_info gr[2];
};

void l3_init(const struct mpeg_header* const header);
int l3_decode_samples(struct decoder_handle* const handle, unsigned frame_count);

#endif // !_MMP_LAYER3_H_
