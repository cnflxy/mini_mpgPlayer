#define _CRT_SECURE_NO_WARNINGS

#include "layer3.h"
#include "newhuffman.h"
#include "synth.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define	SBLIMIT	32
#define SSLIMIT	18

#define M_PI       3.14159265358979323846
#define M_SQRT2	1.41421356237309504880

// scalefactor bit lengths
static const unsigned char sflen_table[2][16] = {
	{ 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 },
	{ 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 }
};

// MPEG-1.0 scalefactor bands index
// __sfb_index_long[sampling_frequency]
static const unsigned short __sfb_index_long[3][23] = {
	{ 0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 52, 62, 74, 90, 110, 134, 162, 196, 238, 288, 342, 418, 576 },	// 44.1kHz
	{ 0, 4, 8, 12, 16, 20, 24, 30, 36, 42, 50, 60, 72, 88, 106, 128, 156, 190, 230, 276, 330, 384, 576 },	// 48kHz
	{ 0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 54, 66, 82, 102, 126, 156, 194, 240, 296, 364, 448, 550, 576 }	// 32kHz
};
// __sfb_index_short[sampling_frequency]
static const unsigned short __sfb_index_short[3][14] = {
	{ 0, 4 * 3, 8 * 3, 12 * 3, 16 * 3, 22 * 3, 30 * 3, 40 * 3, 52 * 3, 66 * 3, 84 * 3, 106 * 3, 136 * 3, 192 * 3 },	// 44.1kHz
	{ 0, 4 * 3, 8 * 3, 12 * 3, 16 * 3, 22 * 3, 28 * 3, 38 * 3, 50 * 3, 64 * 3, 80 * 3, 100 * 3, 126 * 3, 192 * 3 },	// 48kHz
	{ 0, 4 * 3, 8 * 3, 12 * 3, 16 * 3, 22 * 3, 30 * 3, 42 * 3, 58 * 3, 78 * 3, 104 * 3, 138 * 3, 180 * 3, 192 * 3 }	// 32kHz
};

// MPEG-1.0 scalefactor band widths
// __sfb_width_long[sampling_frequency]
static const unsigned short __sfb_width_long[3][22] = {
	{ 4, 4, 4, 4, 4, 4, 6, 6, 8, 8, 10, 12, 16, 20, 24, 28, 34, 42, 50, 54, 76, 158 },	// 44.1kHz
	{ 4, 4, 4, 4, 4, 4, 6, 6, 6, 8, 10, 12, 16, 18, 22, 28, 34, 40, 46, 54, 54, 192 },	// 48kHz
	{ 4, 4, 4, 4, 4, 4, 6, 6, 8, 10, 12, 16, 20, 24, 30, 38, 46, 56, 68, 84, 102, 26 }	// 32kHz
};
// __sfb_width_short[sampling_frequency]
static const unsigned short __sfb_width_short[3][13] = {
	{ 4, 4, 4, 4, 6, 8, 10, 12, 14, 18, 22, 30, 56 },	// 44.1kHz
	{ 4, 4, 4, 4, 6, 6, 10, 12, 14, 16, 20, 26, 66 },	// 48kHz
	{ 4, 4, 4, 4, 6, 8, 12, 16, 20, 26, 34, 42, 12 }	// 32kHz
};

static struct {
	const unsigned short* index_long;
	const unsigned short* index_short;
	const unsigned short* width_long;
	const unsigned short* width_short;
} cur_sfb_table;

// scalefactor band preemphasis (used only when preflag is set)
static const unsigned char pretab[2][21] = {
	{ 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2 }
};

static struct {
	unsigned char band_short[3];
	unsigned char band_long;
} rzero;

// gain_powreq[i] = i^(4/3)
static float gain_powreq[8207];

// gain_pow2_is[i] = 2^i
//static float gain_pow2_is[256 + 118 + 4];


/*
coefficients for aliasing reduction

c[] = { -0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037}
cs[i] = 1 / sqrt(1 + c[i]^2)
ca[i] = c[i] / sqrt(1 + c[i]^2)
*/
static const double __Ci[] = { -0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037 };
static float cs[8], ca[8];

/*
IMDCT coefficients for short blocks and long blocks

imdct_s[i][k] = cos(PI * (2 * i + 7) * (2 * k + 1) / 24) = cos(PI * (i + 3.5) * (k + 0.5) / 12)
imdct_l[i][k] = cos(PI * (2 * i + 19) * (2 * k + 1) / 72) = cos(PI * (i + 9.5) * (k + 0.5) / 36)
*/
static float imdct_s[6][12];
static float imdct_l[18][36];

static float imdct_window[4][36];

#if 0
/*
windowing coefficients for short blocks

window_s[i] = sin(PI * (i + 1/2) / 12) = sin(PI * (2 * i + 1) / 24)
*/
static float window_s[12];

/*
windowing coefficients for long blocks

window_l[i] = sin(PI * (i + 1/2) / 36) = sin(PI * (2 * i + 1) / 72)
*/
static float window_l[36];
#endif

/*
coefficients for intensity stereo processing

is_ratio[i] = tan(i * (PI / 12))
is_table[i] = is_ratio[i] / (1 + is_ratio[i])
*/
static double is_ratio[6];
//static float is_table[7];


static float overlapp[2][SBLIMIT * SSLIMIT];


static int l3_decode_sideinfo(struct bs* const sideinfo_stream, struct l3_sideinfo* const si, const int nch)
{
	int gr, ch, region;
	char log_msg_buf[64];

	si->main_data_begin = bs_readBits(sideinfo_stream, 9);
	si->private_bits = bs_readBits(sideinfo_stream, nch == 1 ? 5 : 3);	// private_bits

	for (ch = 0; ch < nch; ++ch) {
		for (int scfsi_band = 0; scfsi_band < 4; ++scfsi_band)
			si->scfsi[ch][scfsi_band] = bs_readBit(sideinfo_stream);
	}

	for (gr = 0; gr < 2; ++gr) {
		for (ch = 0; ch < nch; ++ch) {
			struct ch_info* const cur_ch = si->gr[gr].ch + ch;

			cur_ch->part2_3_len = bs_readBits(sideinfo_stream, 12);
			if (cur_ch->part2_3_len == 0) {
				sprintf(log_msg_buf, "gr%dch%d's part2_3_len==0!", gr, ch);
				LOG_W("sideinfo_check", log_msg_buf);
			}

			cur_ch->big_values = bs_readBits(sideinfo_stream, 9);
			if (cur_ch->big_values > 288) {
				sprintf(log_msg_buf, "gr%dch%d's big_values==%hu too large!", gr, ch, cur_ch->big_values);
				LOG_W("sideinfo_check", log_msg_buf);
				cur_ch->big_values = 288;
			}

			cur_ch->global_gain = bs_readBits(sideinfo_stream, 8);
			cur_ch->scalefac_compress = bs_readBits(sideinfo_stream, 4);
			if (cur_ch->part2_3_len == 0) {
				if (cur_ch->scalefac_compress) {
					sprintf(log_msg_buf, "gr%dch%d's scalefac_compress==%hu when part2_3_len==0!", gr, ch, cur_ch->scalefac_compress);
					LOG_W("sideinfo_check", log_msg_buf);
					cur_ch->scalefac_compress = 0;
				}
			}

			cur_ch->win_switch_flag = bs_readBit(sideinfo_stream);
			if (cur_ch->win_switch_flag == 1) {
				cur_ch->block_type = bs_readBits(sideinfo_stream, 2);
				if (cur_ch->block_type == 0) {
					LOG_E("sideinfo_check", "block_type==0 when win_switch_flag==1!");
					return -1;
				} else if (cur_ch->block_type == 2 && *(unsigned*)(si->scfsi[ch]) != 0) {
					LOG_E("sideinfo_check", "block_type==2 when scfsi!=0!");
					return -1;
				}

				cur_ch->mixed_block_flag = bs_readBit(sideinfo_stream);
				if (cur_ch->block_type == 2 && cur_ch->mixed_block_flag == 1) {
					LOG_W("sideinfo_check", "block_type==2 when mixed_block_flag==1!");
				}

				for (region = 0; region < 2; ++region)
					cur_ch->table_select[region] = bs_readBits(sideinfo_stream, 5);
				cur_ch->table_select[2] = 0;
				for (int window = 0; window < 3; ++window)
					cur_ch->subblock_gain[window] = bs_readBits(sideinfo_stream, 3);

				if (cur_ch->block_type == 2 && cur_ch->mixed_block_flag == 0)
					cur_ch->region0_count = 8;
				else cur_ch->region0_count = 7;
				cur_ch->region1_count = 20 - cur_ch->region0_count;

				//cur_ch->region0_count = 36 >> 1;
				//cur_ch->region1_count = 576 >> 1;
			} else {
				cur_ch->block_type = 0;
				cur_ch->mixed_block_flag = 0;

				for (region = 0; region < 3; ++region)
					cur_ch->table_select[region] = bs_readBits(sideinfo_stream, 5);

				//int r1 = bs_readBits(decode_stream, 4) + 1, r2 = bs_readBits(decode_stream, 3) + 1;
				//cur_ch->region0_count = cur_sfb_table.index_long[r1] >> 1;
				//if (r1 + r2 > 22)
				//	cur_ch->region1_count = 576 >> 1;
				//else cur_ch->region1_count = cur_sfb_table.index_long[r1 + r2] >> 1;

				cur_ch->region0_count = bs_readBits(sideinfo_stream, 4);
				cur_ch->region1_count = bs_readBits(sideinfo_stream, 3);
			}
			cur_ch->preflag = bs_readBit(sideinfo_stream);
			cur_ch->scalefac_scale = bs_readBit(sideinfo_stream);
			cur_ch->count1table_select = bs_readBit(sideinfo_stream);
		}
	}

	return 0;
}

static void l3_decode_scalefactors(struct bs* const maindata_stream, struct ch_info* const cur_ch, const struct l3_sideinfo* const si, const int gr, const int ch)
{
	const unsigned char slen0 = sflen_table[0][cur_ch->scalefac_compress];
	const unsigned char slen1 = sflen_table[1][cur_ch->scalefac_compress];
	int sb;

	if (cur_ch->part2_3_len == 0) {
		for (sb = 0; sb < 21; ++sb) {
			cur_ch->scalefac_l[sb] = 0;
			cur_ch->scalefac_s[sb] = 0;
		}
		for (sb = 23; sb < 36; ++sb)
			cur_ch->scalefac_s[sb] = 0;
		cur_ch->part2_len = 0;
		return;
	}

	if (cur_ch->win_switch_flag == 1 && cur_ch->block_type == 2) {
		if (cur_ch->mixed_block_flag == 1) {
			// MIXED block
			cur_ch->part2_len = slen0 * 17 + slen1 * 18;
			for (sb = 0; sb < 8; ++sb)
				cur_ch->scalefac_l[sb] = bs_readBits(maindata_stream, slen0);
			for (sb = 9; sb < 18; ++sb)
				cur_ch->scalefac_s[sb] = bs_readBits(maindata_stream, slen0);
			for (sb = 18; sb < 36; ++sb)
				cur_ch->scalefac_s[sb] = bs_readBits(maindata_stream, slen1);
		} else {
			// pure SHORT block
			cur_ch->part2_len = (slen0 + slen1) * 18;
			for (sb = 0; sb < 18; ++sb)
				cur_ch->scalefac_s[sb] = bs_readBits(maindata_stream, slen0);
			for (sb = 18; sb < 36; ++sb)
				cur_ch->scalefac_s[sb] = bs_readBits(maindata_stream, slen1);
		}
	} else {
		// LONG types 0,1,3
		cur_ch->part2_len = 0;
		/* Scale factor bands 0-5 */
		if (si->scfsi[ch][0] == 0 || gr == 0) {
			for (sb = 0; sb < 6; ++sb)
				cur_ch->scalefac_l[sb] = bs_readBits(maindata_stream, slen0);
			cur_ch->part2_len += slen0 * 6;
		}
		//} else {
		//	/* Copy scalefactors from granule 0 to granule 1 */
		//	for (sb = 0; sb < 6; ++sb)
		//		cur_ch->scalefac_l[sb] = si->gr[0].ch[ch].scalefac_l[sb];
		//}

		/* Scale factor bands 6-10 */
		if (si->scfsi[ch][1] == 0 || gr == 0) {
			for (sb = 6; sb < 11; ++sb)
				cur_ch->scalefac_l[sb] = bs_readBits(maindata_stream, slen0);
			cur_ch->part2_len += slen0 * 5;
		}
		//} else {
		//	/* Copy scalefactors from granule 0 to granule 1 */
		//	for (sb = 6; sb < 11; ++sb)
		//		cur_ch->scalefac_l[sb] = si->gr[0].ch[ch].scalefac_l[sb];
		//}

		/* Scale factor bands 11-15 */
		if (si->scfsi[ch][2] == 0 || gr == 0) {
			for (sb = 11; sb < 16; ++sb)
				cur_ch->scalefac_l[sb] = bs_readBits(maindata_stream, slen1);
			cur_ch->part2_len += slen1 * 5;
		}
		//} else {
		//	/* Copy scalefactors from granule 0 to granule 1 */
		//	for (sb = 11; sb < 16; ++sb)
		//		cur_ch->scalefac_l[sb] = si->gr[0].ch[ch].scalefac_l[sb];
		//}

		/* Scale factor bands 16-20 */
		if (si->scfsi[ch][3] == 0 || gr == 0) {
			for (sb = 16; sb < 21; ++sb)
				cur_ch->scalefac_l[sb] = bs_readBits(maindata_stream, slen1);
			cur_ch->part2_len += slen1 * 5;
		}
		//} else {
		//	/* Copy scalefactors from granule 0 to granule 1 */
		//	for (sb = 16; sb < 21; ++sb)
		//		cur_ch->scalefac_l[sb] = si->gr[0].ch[ch].scalefac_l[sb];
		//}
	}
}

static void l3_huffman_decode(struct bs* const maindata_stream, struct ch_info* const cur_ch, short is[SBLIMIT * SSLIMIT])
{
	int region[3], r, is_pos = 0, bv = cur_ch->big_values * 2, part3_len = 0;
	const struct huff_tab* htab;
	unsigned point, bitleft;
	short huff_code[4];

	if (cur_ch->part2_3_len != 0) {
		part3_len = cur_ch->part2_3_len - cur_ch->part2_len;

		{
			if (cur_ch->win_switch_flag == 1 && cur_ch->block_type == 2) {
				region[0] = 36;
				region[1] = 576 - 2;
			} else {
				int r1 = cur_ch->region0_count + 1, r2 = r1 + cur_ch->region1_count + 1;
				if (r2 > 22) r2 = 22;
				region[0] = cur_sfb_table.index_long[r1];
				region[1] = cur_sfb_table.index_long[r2];
			}

			if (bv <= region[0])
				region[0] = region[1] = bv > 574 ? 574 : bv;
			else if (bv <= region[1])
				region[1] = bv > 574 ? 574 : bv;
			region[2] = bv > 574 ? 574 : bv;
		}

		// 解码大值区
		for (r = is_pos = 0; r < 3; ++r) {
			htab = ht + cur_ch->table_select[r];
			for (; is_pos < region[r] && part3_len > 0; ++is_pos) {
				point = bitleft = 0;
				huff_code[0] = huff_code[1] = huff_code[2] = huff_code[3] = 0;
				do {
					if ((htab->table[point] & 0xff00) == 0) {
						huff_code[0] = (htab->table[point] >> 4) & 0xf;
						huff_code[1] = htab->table[point] & 0xf;
						break;
					}

					if (bs_readBit(maindata_stream) == 1) { /* goto right-child*/
						while ((htab->table[point] & 0xff) >= 250)
							point += htab->table[point] & 0xff;
						point += htab->table[point] & 0xff;
					} else { /* goto left-child*/
						while ((htab->table[point] >> 8) >= 250)
							point += htab->table[point] >> 8;
						point += htab->table[point] >> 8;
					}
					--part3_len;
				} while (++bitleft < 32 && part3_len > 0);

				// get linbits
				if (htab->linbits > 0 && huff_code[0] == 15) {
					huff_code[0] += bs_readBits(maindata_stream, htab->linbits);
					if ((part3_len -= htab->linbits) < 0)
						break;
				}
				// get sign bit
				if (huff_code[0] > 0) {
					if (bs_readBit(maindata_stream) == 1)
						huff_code[0] = -huff_code[0];
					if (--part3_len < 0)
						break;
				}
				is[is_pos++] = huff_code[0];

				// get linbits
				if (htab->linbits > 0 && huff_code[1] == 15) {
					huff_code[1] += bs_readBits(maindata_stream, htab->linbits);
					if ((part3_len -= htab->linbits) < 0) {
						is_pos -= 1;
						break;
					}
				}
				// get sign bit
				if (huff_code[1] > 0) {
					if (bs_readBit(maindata_stream) == 1)
						huff_code[1] = -huff_code[1];
					if (--part3_len < 0) {
						is_pos -= 1;
						break;
					}
				}
				is[is_pos] = huff_code[1];
			}
		}

		// 解码小值区
		htab = htc + cur_ch->count1table_select;
		for (; is_pos <= 572 && part3_len > 0; ++is_pos) {
			point = bitleft = 0;
			huff_code[0] = huff_code[1] = huff_code[2] = huff_code[3] = 0;
			do {
				if ((htab->table[point] & 0xff00) == 0) {
					huff_code[0] = htab->table[point] & 0xf;
					break;
				}

				if (bs_readBit(maindata_stream) == 1) { /* goto right-child*/
					while ((htab->table[point] & 0xff) >= 250)
						point += htab->table[point] & 0xff;
					point += htab->table[point] & 0xff;
				} else { /* goto left-child*/
					while ((htab->table[point] >> 8) >= 250)
						point += htab->table[point] >> 8;
					point += htab->table[point] >> 8;
				}
				--part3_len;
			} while (++bitleft < 32 && part3_len > 0);

			huff_code[3] = (huff_code[0] >> 3) & 0x1;
			huff_code[2] = (huff_code[0] >> 2) & 0x1;
			huff_code[1] = (huff_code[0] >> 1) & 0x1;
			huff_code[0] = (huff_code[0] >> 0) & 0x1;

			if (huff_code[3] > 0) {
				if (bs_readBit(maindata_stream) == 1)
					huff_code[3] = -huff_code[3];
				if (--part3_len < 0) {
					break;
				}
			}
			is[is_pos++] = huff_code[3];

			if (huff_code[2] > 0) {
				if (bs_readBit(maindata_stream) == 1)
					huff_code[2] = -huff_code[2];
				if (--part3_len < 0) {
					is_pos -= 1;
					break;
				}
			}
			is[is_pos++] = huff_code[2];

			if (huff_code[1] > 0) {
				if (bs_readBit(maindata_stream) == 1)
					huff_code[1] = -huff_code[1];
				if (--part3_len < 0) {
					is_pos -= 2;
					break;
				}
			}
			is[is_pos++] = huff_code[1];

			if (huff_code[0] > 0) {
				if (bs_readBit(maindata_stream) == 1)
					huff_code[0] = -huff_code[0];
				if (--part3_len < 0) {
					is_pos -= 3;
					break;
				}
			}
			is[is_pos] = huff_code[0];
		}
	}

	//if ((maindata_stream->byte_ptr - maindata_stream->bit_buf) * 8 + maindata_stream->bit_pos > bit_pos_end + 1) {
	//	is_pos -= 4;
	//}

	if (part3_len < 0) {
		// is_pos -= 4;
		// bs_backBits(maindata_stream, -part3_len);
	} else if (part3_len > 0)
		bs_skipBits(maindata_stream, part3_len);

	cur_ch->nonzero_len = is_pos;

	while (is_pos < 576)
		is[is_pos++] = 0;
}

static void l3_requantize_long(const struct ch_info* const cur_ch, const int sfb, const int pos, const short is[SBLIMIT * SSLIMIT], float xr[SBLIMIT * SSLIMIT])
{
	const double sf_mult = cur_ch->scalefac_scale != 0 ? 1.0 : 0.5;
	float tmp1, tmp2, tmp3;

	if (sfb < 21)
		tmp1 = (float)pow(2.0, -(sf_mult * ((double)cur_ch->scalefac_l[sfb] + pretab[cur_ch->preflag][sfb])));
	else tmp1 = 1.0f;

	tmp2 = (float)pow(2.0, 0.25 * (cur_ch->global_gain - 210.0));

	if (is[pos] < 0)
		tmp3 = -gain_powreq[-is[pos]];
	else tmp3 = gain_powreq[is[pos]];

	xr[pos] = tmp1 * tmp2 * tmp3;
}

static void l3_requantize_short(const struct ch_info* const cur_ch, const int sfb, const int window, const int pos, const short is[SBLIMIT * SSLIMIT], float xr[SBLIMIT * SSLIMIT])
{
	const double sf_mult = cur_ch->scalefac_scale != 0 ? 1.0 : 0.5;
	float tmp1, tmp2, tmp3;

	if (sfb < 12)
		tmp1 = (float)pow(2.0, -(sf_mult * cur_ch->scalefac_s[sfb * 3 + window]));
	else tmp1 = 1.0f;

	tmp2 = (float)pow(2.0, 0.25 * (cur_ch->global_gain - 210.0 - cur_ch->subblock_gain[window] * 8.0));

	if (is[pos] < 0)
		tmp3 = -gain_powreq[-is[pos]];
	else tmp3 = gain_powreq[is[pos]];

	xr[pos] = tmp1 * tmp2 * tmp3;
}

//static void l3_reorder(const struct ch_info* const cur_ch, const short is[SBLIMIT * SSLIMIT], float xr[SBLIMIT * SSLIMIT])
//{
//	int sfb = 0, next_sfb, is_pos = 0, window, width, i;
//
//
//}

static void l3_requantize(const struct ch_info* const cur_ch, const short is[SBLIMIT * SSLIMIT], float xr[SBLIMIT * SSLIMIT])
{
	int sfb = 0, next_sfb, is_pos = 0, window, width, i;
	float re[SBLIMIT * SSLIMIT];

	if (cur_ch->nonzero_len != 0) {
		if (cur_ch->win_switch_flag == 1 && cur_ch->block_type == 2) {
			if (cur_ch->mixed_block_flag == 1) { /* MIXED BLOCk*/
				for (is_pos = 0, sfb = 0, next_sfb = cur_sfb_table.index_long[sfb + 1]; is_pos < 36; ++is_pos) {
					if (is_pos == next_sfb) {
						++sfb;
						next_sfb = cur_sfb_table.index_long[sfb + 1];
					}

					l3_requantize_long(cur_ch, sfb, is_pos, is, xr);
				}

				for (is_pos = 36, sfb = 3, next_sfb = cur_sfb_table.index_short[sfb + 1], width = cur_sfb_table.width_short[sfb]; is_pos < cur_ch->nonzero_len;) {
					if (is_pos == next_sfb) {
						for (i = 0; i < 3 * width; ++i) {
							xr[cur_sfb_table.index_short[12] + sfb * 3 + i] = re[i];
						}
						++sfb;
						next_sfb = cur_sfb_table.index_short[sfb + 1];
						width = cur_sfb_table.width_short[sfb];
					}
					for (window = 0; window < 3; ++window) {
						for (i = 0; i < width; ++i) {
							l3_requantize_short(cur_ch, sfb, window, is_pos, is, xr);
							re[i * 3 + window] = xr[is_pos];
							++is_pos;
						}
					}
				}
			} else { /* pure SHORT BLOCK */
				for (is_pos = 0, sfb = 0, next_sfb = cur_sfb_table.index_short[sfb + 1], width = cur_sfb_table.width_short[sfb]; is_pos < cur_ch->nonzero_len;) {
					if (is_pos == next_sfb) {
						for (i = 0; i < 3 * width; ++i) {
							xr[cur_sfb_table.index_short[12] + sfb * 3 + i] = re[i];
						}
						++sfb;
						next_sfb = cur_sfb_table.index_short[sfb + 1];
						width = cur_sfb_table.width_short[sfb];
					}
					for (window = 0; window < 3; ++window) {
						for (i = 0; i < width; ++i) {
							l3_requantize_short(cur_ch, sfb, window, is_pos, is, xr);
							re[i * 3 + window] = xr[is_pos];
							++is_pos;
						}
					}
				}
			}

			for (i = 0; i < 3 * width; ++i) {
				xr[cur_sfb_table.index_short[12] + sfb * 3 + i] = re[i];
			}
		} else { /* pure LONG BLOCK */
			for (is_pos = 0, sfb = 0, next_sfb = cur_sfb_table.index_long[sfb + 1]; is_pos < cur_ch->nonzero_len; ++is_pos) {
				if (is_pos == next_sfb) {
					++sfb;
					next_sfb = cur_sfb_table.index_long[sfb + 1];
				}

				l3_requantize_long(cur_ch, sfb, is_pos, is, xr);
			}
		}
	}

	// 不逆量化0值区,置0.
	for (; is_pos < 576; ++is_pos)
		xr[is_pos] = 0.0;
}

static void l3_do_ms_stereo(struct gr_info* const cur_gr, const int gr, float xr[2][2][SBLIMIT * SSLIMIT])
{
	int max_len = max(cur_gr->ch[0].nonzero_len, cur_gr->ch[1].nonzero_len);
	//cur_gr->ch[0].nonzero_len = cur_gr->ch[1].nonzero_len = max_len;

	//const double v = sqrt(2.0);
	for (int i = 0; i < max_len; ++i) {
		const float v0 = (float)(((double)xr[0][gr][i] + xr[1][gr][i]) / M_SQRT2);
		const float v1 = (float)(((double)xr[0][gr][i] - xr[1][gr][i]) / M_SQRT2);
		xr[0][gr][i] = v0;
		xr[1][gr][i] = v1;
	}
}

static void do_intensity_stereo_long();
static void l3_do_intesity_stereo(struct gr_info* const cur_gr, const int gr, float xr[2][2][SBLIMIT * SSLIMIT])
{
	int sfb, is_possb, width, sfb_start, sfb_stop, i, window;
	double is_ratio_l, is_ratio_r;

	if (cur_gr->ch[0].win_switch_flag == 1 && cur_gr->ch[0].block_type == 2) {
		// MPEG-1, short block/mixed block
		if (cur_gr->ch[0].mixed_block_flag == 1) {
			for (sfb = 0; sfb < 8; ++sfb) {
				if (cur_sfb_table.index_long[sfb] < cur_gr->ch[1].nonzero_len)
					continue;
				if ((is_possb = cur_gr->ch[0].scalefac_l[sfb]) == 7)
					continue;
				sfb_start = cur_sfb_table.index_long[sfb];
				sfb_stop = cur_sfb_table.index_long[sfb + 1];
				if (is_possb == 6) {
					is_ratio_l = 1.0;
					is_ratio_r = 0.0;
				} else {
					is_ratio_l = is_ratio[is_possb] / (1.0 + is_ratio[is_possb]);
					is_ratio_r = 1.0 / (1.0 + is_ratio[is_possb]);
				}
				for (i = sfb_start; i < sfb_stop; ++i) {
					xr[0][gr][i] *= (float)is_ratio_l;
					xr[1][gr][i] *= (float)is_ratio_r;
				}
			}

			for (sfb = 3; sfb < 12; ++sfb) {
				if (cur_sfb_table.index_short[sfb] < cur_gr->ch[1].nonzero_len)
					continue;
				width = cur_sfb_table.width_short[sfb];
				for (window = 0; window < 3; ++window) {
					if ((is_possb = cur_gr->ch[0].scalefac_s[sfb * 3 + window]) == 7)
						continue;
					sfb_start = cur_sfb_table.index_short[sfb] + width * window;
					sfb_stop = sfb_start + width;
					if (is_possb == 6) {
						is_ratio_l = 1.0;
						is_ratio_r = 0.0;
					} else {
						is_ratio_l = is_ratio[is_possb] / (1.0 + is_ratio[is_possb]);
						is_ratio_r = 1.0 / (1.0 + is_ratio[is_possb]);
					}
					for (i = sfb_start; i < sfb_stop; ++i) {
						xr[0][gr][i] *= (float)is_ratio_l;
						xr[1][gr][i] *= (float)is_ratio_r;
					}
				}
			}
		} else {
			for (sfb = 0; sfb < 12; ++sfb) {
				if (cur_sfb_table.index_short[sfb] < cur_gr->ch[1].nonzero_len)
					continue;
				width = cur_sfb_table.width_short[sfb];
				for (window = 0; window < 3; ++window) {
					if ((is_possb = cur_gr->ch[0].scalefac_s[sfb * 3 + window]) == 7)
						continue;
					sfb_start = cur_sfb_table.index_short[sfb] + width * window;
					sfb_stop = sfb_start + width;
					if (is_possb == 6) {
						is_ratio_l = 1.0;
						is_ratio_r = 0.0;
					} else {
						is_ratio_l = is_ratio[is_possb] / (1.0 + is_ratio[is_possb]);
						is_ratio_r = 1.0 / (1.0 + is_ratio[is_possb]);
					}
					for (i = sfb_start; i < sfb_stop; ++i) {
						xr[0][gr][i] *= (float)is_ratio_l;
						xr[1][gr][i] *= (float)is_ratio_r;
					}
				}
			}
		}
	} else {
		// MPEG-1, long block
		for (sfb = 0; sfb < 21; ++sfb) {
			if (cur_sfb_table.index_long[sfb] < cur_gr->ch[1].nonzero_len)
				continue;
			if ((is_possb = cur_gr->ch[0].scalefac_l[sfb]) == 7)
				continue;
			sfb_start = cur_sfb_table.index_long[sfb];
			sfb_stop = cur_sfb_table.index_long[sfb + 1];
			if (is_possb == 6) {
				is_ratio_l = 1.0;
				is_ratio_r = 0.0;
			} else {
				is_ratio_l = is_ratio[is_possb] / (1.0 + is_ratio[is_possb]);
				is_ratio_r = 1.0 / (1.0 + is_ratio[is_possb]);
			}
			for (i = sfb_start; i < sfb_stop; ++i) {
				xr[0][gr][i] *= (float)is_ratio_l;
				xr[1][gr][i] *= (float)is_ratio_r;
			}
		}
	}
}

static void l3_do_stereo(const struct mpeg_frame* frame, struct gr_info* const cur_gr, const int gr, float xr[2][2][SBLIMIT * SSLIMIT])
{
	if (frame->is_MS)
		l3_do_ms_stereo(cur_gr, gr, xr);

	if (cur_gr->ch[0].mixed_block_flag != cur_gr->ch[1].mixed_block_flag || cur_gr->ch[0].block_type != cur_gr->ch[1].block_type) {
		LOG_W("check_stereo", "bad stereo!");
	} else if (frame->is_Intensity)
		l3_do_intesity_stereo(cur_gr, gr, xr);
}

static void l3_antialias(const struct ch_info* const cur_ch, float xr[SBLIMIT * SSLIMIT])
{
	int sblimit;

	if (cur_ch->win_switch_flag == 1 && cur_ch->block_type == 2) {
		if (cur_ch->mixed_block_flag == 0)
			return;
		sblimit = 2 * 18;
	} else
		sblimit = 32 * 18;

	for (int sb = 1 * 18; sb < sblimit; sb += 18) {
		for (int i = 0; i < 8; ++i) {
			const float lb = xr[sb - i - 1];
			const float ub = xr[sb + i];
			xr[sb - i - 1] = lb * cs[i] - ub * ca[i];
			xr[sb + i] = ub * cs[i] + lb * ca[i];
		}
	}
}

static void imdct12(const float xr[18], float rawout[36])
{
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 12; ++j) {
			rawout[6 * i + j + 6] = 0.0f;
			for (int k = 0; k < 6; ++k) {
				rawout[6 * i + j + 6] += xr[i + 3 * k] * imdct_s[k][j] * imdct_window[2][j];
			}
		}
	}
}

static void imdct36(const float xr[18], float rawout[36], unsigned char block_type)
{
	for (int i = 0; i < 36; ++i) {
		rawout[i] = 0.0f;
		for (int j = 0; j < 18; ++j) {
			rawout[i] += xr[j] * imdct_l[j][i] * imdct_window[block_type][i];
		}
	}
}

static void l3_hybrid(const struct ch_info* const cur_ch, const int ch, float xr[SBLIMIT * SSLIMIT])
{
	float rawout[36];
	int off;

	for (off = 0; off < SBLIMIT * SSLIMIT && off < cur_ch->nonzero_len; off += SSLIMIT) {
		unsigned char block_type = (cur_ch->win_switch_flag == 1 && cur_ch->mixed_block_flag == 1 && off < 2 * SSLIMIT) ? 0 : cur_ch->block_type;

		/* IMDCT and WINDOWING */
		if (block_type == 2)
			imdct12(xr + off, rawout);
		else
			imdct36(xr + off, rawout, block_type);

		/* OVERLAPPING */
		for (int i = 0; i < SSLIMIT; ++i) {
			xr[off + i] = rawout[i] + overlapp[ch][off + i];
			overlapp[ch][off + i] = rawout[i + 18];
		}
	}

	// 0值区
	//for (off = cur_ch->nonzero_len; off < 576; ++off) {
	//	xr[off] = overlapp[ch][off];
	//	overlapp[ch][off] = 0.0;
	//}
}

void l3_init(const struct mpeg_header* const header)
{
	cur_sfb_table.index_long = __sfb_index_long[header->sampling_frequency];
	cur_sfb_table.index_short = __sfb_index_short[header->sampling_frequency];
	cur_sfb_table.width_long = __sfb_width_long[header->sampling_frequency];
	cur_sfb_table.width_short = __sfb_width_short[header->sampling_frequency];

	int i, k;
	for (i = 0; i < 8207; ++i)
		gain_powreq[i] = (float)pow((double)i, 4.0 / 3.0);

	//for (i = -256; i < 118 + 4; ++i)
	//	gain_pow2_is[i + 256] = (float)pow(2.0, (i + 210.0) * -1.0 / 4.0);

	for (i = 0; i < 8; ++i) {
		double sq = sqrt(1.0 + __Ci[i] * __Ci[i]);
		cs[i] = (float)(1.0 / sq);
		ca[i] = (float)(__Ci[i] / sq);
	}

	{
		/* blocktype 0*/
		for (i = 0; i < 36; ++i) {
			imdct_window[0][i] = (float)sin(M_PI * (i + 0.5) / 36.0);
		}
		/* Blocktype 1 */
		for (i = 0; i < 18; ++i) {
			imdct_window[1][i] = (float)sin(M_PI * (i + 0.5) / 36.0);
		}
		for (i = 18; i < 24; ++i) {
			imdct_window[1][i] = 1.0f;
		}
		for (i = 24; i < 30; ++i) {
			imdct_window[1][i] = (float)sin(M_PI * (i + 0.5 - 18.0) / 12.0);
		}
		/* Blocktype 2 */
		for (i = 0; i < 12; ++i) {
			imdct_window[2][i] = (float)sin(M_PI * (i + 0.5) / 12.0);
		}
		/* Blocktype 3 */
		for (i = 6; i < 12; ++i) {
			imdct_window[3][i] = (float)sin(M_PI * (i + 0.5 - 6.0) / 12.0);
		}
		for (i = 12; i < 18; ++i) {
			imdct_window[3][i] = 1.0;
		}
		for (i = 18; i < 36; ++i) {
			imdct_window[3][i] = (float)sin(M_PI * (i + 0.5) / 36.0);
		}
	}

	for (i = 0; i < 6; ++i) {
		for (k = 0; k < 12; ++k) {
			imdct_s[i][k] = (float)cos(M_PI * (2.0 * k + 7.0) * (2.0 * i + 1.0) / 24.0);
		}
		// window_s[i] = sin(PI * (2.0 * i + 1.0) / 24.0);
	}

	for (i = 0; i < 18; ++i) {
		for (k = 0; k < 36; ++k) {
			imdct_l[i][k] = (float)cos(M_PI * (2.0 * k + 19.0) * (2.0 * i + 1.0) / 72.0);
		}
		// window_l[i] = sin(PI * (2.0 * i + 1.0) / 72.0);
	}

	// static float is_coef[] = { 0.0, 0.211324865, 0.366025404, 0.5, 0.633974596, 0.788675135, 1.0 };
	//float is_ratio;
	//for (i = 0; i < 7; ++i) {
	//	is_ratio = tan(i * PI / 12.0);
	//	is_table[i] = (float)(is_ratio / (1.0 + is_ratio));
	//}
	for (i = 0; i < 6; ++i) {
		is_ratio[i] = (float)tan(i * M_PI / 12.0);
	}

	//float q = sqrt(2);	// # define M_SQRT2	1.41421356237309504880
	//for (i = 0; i < 16; ++i) {
	//	float t = tan(i * M_PI / 12.0);
	//	_intesity_tabs[0][0][i] = t / (1.0 + t);
	//	_intesity_tabs[0][1][i] = 1.0 / (1.0 + t);
	//	_intesity_tabs[1][0][i] = q * t / (1.0 + t);
	//	_intesity_tabs[1][1][i] = q / (1.0 + t);
	//}

	init_synthesis_tabs();
}

static short is[SBLIMIT * SSLIMIT];
static float xr[2][2][SBLIMIT * SSLIMIT];
int l3_decode_samples(struct decoder_handle* handle, unsigned frame_count)
{
	const struct mpeg_frame* const cur_frame = &handle->cur_frame;
	struct bs* const file_stream = handle->file_stream;
	struct bs* const sideinfo_stream = handle->sideinfo_stream;
	struct bs* const maindata_stream = handle->maindata_stream;
	struct l3_sideinfo sideinfo = { 0 };
	char log_msg_buf[64];


	sideinfo_stream->byte_ptr = file_stream->byte_ptr;
	sideinfo_stream->bit_pos = 0;
	if (l3_decode_sideinfo(sideinfo_stream, &sideinfo, cur_frame->nch) == -1) {
		sprintf(log_msg_buf, "frame#%u skipped(decode sideinfo failed)!", frame_count);
		LOG_E("l3_decode_sideinfo", log_msg_buf);
		return 1;
	}

	if (bs_Length(maindata_stream) < sideinfo.main_data_begin) {
		sprintf(log_msg_buf, "frame#%u maindata miss!", frame_count);
		LOG_E("adjust_maindata", log_msg_buf);
		if (bs_Append(maindata_stream, sideinfo_stream->byte_ptr, 0, cur_frame->maindata_size) != cur_frame->maindata_size) {
			sprintf(log_msg_buf, "frame#%u maindata_stream overflow!", frame_count);
			LOG_E("bs_Append(maindata_stream)", log_msg_buf);
		}
		return 1;
	}

	int discard = bs_Avaliable(maindata_stream) - sideinfo.main_data_begin;
	bs_skipBytes(maindata_stream, discard);
	//maindata_stream->byte_ptr -= sideinfo.main_data_begin;
	// maindata_stream->bit_pos = 0;

	if (bs_Append(maindata_stream, sideinfo_stream->byte_ptr, 0, cur_frame->maindata_size) != cur_frame->maindata_size) {
		sprintf(log_msg_buf, "frame#%u maindata_stream overflow!", frame_count);
		LOG_E("bs_Append(maindata_stream)", log_msg_buf);
		return 1;
	}

	for (int gr = 0; gr < 2; ++gr) {
		for (int ch = 0; ch < cur_frame->nch; ++ch) {
			struct ch_info* const cur_ch = &sideinfo.gr[gr].ch[ch];
			l3_decode_scalefactors(maindata_stream, cur_ch, &sideinfo, gr, ch);
			l3_huffman_decode(maindata_stream, cur_ch, is);
			l3_requantize(cur_ch, is, xr[ch][gr]);
		}

		if (cur_frame->nch == 2)
			l3_do_stereo(cur_frame, &sideinfo.gr[gr], gr, xr);

		for (int ch = 0; ch < cur_frame->nch; ++ch) {
			struct ch_info* const cur_ch = &sideinfo.gr[gr].ch[ch];
			l3_antialias(cur_ch, xr[ch][gr]);
			l3_hybrid(cur_ch, ch, xr[ch][gr]);

			/* frequency inversion */
			for (int sb = 1; sb < 32; sb += 2) {
				for (int i = 1; i < 18; i += 2) {
					xr[ch][gr][sb * 18 + i] = -xr[ch][gr][sb * 18 + i];
				}
			}

			/* polyphase subband synthesis */
			synthesis_subband_filter(xr[ch][gr], &handle->pcm, ch, cur_frame->nch);
		}
	}





	//int maindata_len = maindata_stream->end_ptr - maindata_stream->bit_buf;
	//if (maindata_len < sideinfo.main_data_begin) {
	//	fprintf(stderr, "[W] frame#%u skipped(need more maindata)!\n", cur_frame_id);
	//	if (bs_append(maindata_stream, decode_stream->byte_ptr, 0, frame->maindata_size) != frame->maindata_size) {
	//		fprintf(stderr, "[E] maindata stream overflow!\n");
	//		return -1;
	//	}
	//	return 1;
	//}

	//int skip_size = maindata_len - (maindata_stream->byte_ptr - maindata_stream->bit_reservoir) - sideinfo.main_data_begin, ret;
	//// printf("%u <-> %d <-> %d <-> %d <-> %d\n", frame->maindata_size, maindata_len, maindata_stream->byte_ptr - maindata_stream->bit_reservoir, sideinfo->main_data_end, skip_size);
	//if ((ret = bs_skipBytes(maindata_stream, skip_size)) != skip_size) {
	//	fprintf(stderr, "[E] frame #%u (bs_skipBytes(%d) returned %d)!\n", cur_frame_id, skip_size, ret);
	//	return -1;
	//}

	//if (bs_append(maindata_stream, decode_stream->byte_ptr, 0, frame->maindata_size) != frame->maindata_size) {
	//	fprintf(stderr, "[E] maindata stream overflow!\n");
	//	return -1;
	//}

	//for (int gr = 0; gr < 2; ++gr) {
	//	for (int ch = 0; ch < frame->nch; ++ch) {
	//		//struct ch_info* cur_ch = &sideinfo.info_ch;

	//		l3_decode_scalefactors(maindata_stream, &sideinfo, gr, ch);
	//		if (!l3_huffman_decode(maindata_stream, &sideinfo, gr, ch)) {
	//			fprintf(stderr, "[W] nonzero_len == 0\n");
	//		}

	//		l3_requantize_samples(frame, &sideinfo, gr, ch);
	//	}

	//	if (frame->is_MS || frame->is_Intensity) {
	//		l3_do_stereo(frame, &sideinfo, gr);
	//	}


	//}

	//static float samples_tmp[2][SBLIMIT];
	//for (int ch = 0; ch < frame->nch; ++ch) {
	//	for (int gr = 0; gr < 2; ++gr) {
	//		struct ch_info* cur_ch = &sideinfo.info_ch[ch][gr];
	//		l3_antialias(cur_ch);
	//		l3_hybrid(cur_ch, ch);

	//		for (int ss = 0, i, sb; ss < SSLIMIT; ss += 2) {
	//			for (i = ss, sb = 0; sb < 32; ++sb, i += 18)
	//				samples_tmp[ch][sb] = cur_ch->buf[i];
	//			synthesis_subband_filter(samples_tmp[ch], pcm_out, write_ptr, ch, frame->nch);

	//			for (i = ss + 1, sb = 0; sb < 32; sb += 2, i += 36) {
	//				samples_tmp[ch][sb] = cur_ch->buf[i];

	//				samples_tmp[ch][sb + 1] = -cur_ch->buf[i + 18];	//多相频率倒置(INVERSE QUANTIZE SAMPLES)
	//			}
	//			synthesis_subband_filter(samples_tmp[ch], pcm_out, write_ptr, ch, frame->nch);
	//		}
	//	}
	//}

	//if (frame->is_MS || frame->is_Intensity) {
	//	if (sideinfo->ch[0].gr[gr].nonzero_len <= sideinfo->ch[1].gr[gr].nonzero_len)
	//		sideinfo->ch[0].gr[gr].nonzero_len = sideinfo->ch[1].gr[gr].nonzero_len;
	//	else sideinfo->ch[1].gr[gr].nonzero_len = sideinfo->ch[0].gr[gr].nonzero_len;

	//	float* in[2] = { (float*)l3_samples_in[0], (float*)l3_samples_in[1] };
	//	for (int i = SSLIMIT * sideinfo->ch[0].gr[gr].nonzero_len; i; --i) {
	//		*in[0] += *in[1];
	//		++in[0];
	//		++in[1];
	//	}
	//}

	return 0;
}
