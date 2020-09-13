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
	uint16_t part2_3_len;			// 12 bits, ������λ��, part2_len(scalefactor, ��������λ��) + part3_len(Huffman code, ������������λ��)
	uint16_t big_values;			// 9 bits
	uint16_t global_gain;			// 8 bits
	uint16_t scalefac_compress;	// 4 bits

	uint8_t win_switch_flag;	// 1 bit
	uint8_t block_type;		// 2 bits, win_switch_flag == 1(1/3: ��Ͽ�(1: ��ʼ��, 3: ������), 2: �̿�), win_switch_flag == 0(0: ����)
	uint8_t mixed_block_flag;	// 1 bit, win_switch_flag == 1
	uint8_t table_select[3];	// 2*5bits(win_switch_flag == 1), 3*5bits(win_switch_flag == 0), table_select[region]
	uint8_t subblock_gain[3];	// 3*3bits, win_switch_flag == 1, subblock_gain[window]
	uint8_t region0_count;	// 4 bits, win_switch_flag == 0
	uint8_t region1_count;	// 3 bits, win_switch_flag == 0

	uint8_t preflag;				// 1 bit
	uint8_t scalefac_scale;		// 1 bit
	uint8_t count1table_select;	// 1 bit

	uint16_t part2_len;	// scalefactor len
	uint32_t nonzero_len;	// Huffman������ֵ����Сֵ����
};

struct gr_info {
	struct ch_info ch[2];
};

struct l3_sideinfo {
	uint16_t main_data_begin;	// 9 bits, ָ����������ͬ����֮ǰ���ٸ��ֽڿ�ʼ
	uint8_t private_bits;		// mono: 5 bits, dual: 3 bits

	uint8_t scfsi[2][4];	// 4*1 bits, scfsi[ch][scfsi_band], scale-factor selector information[ band]
	struct gr_info gr[2];
};

void l3_init(const struct mpeg_header* const header);
int l3_decode_samples(struct decoder_handle* const handle, uint32_t frame_count);

#endif // !_MMP_LAYER3_H_
