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
static float gain_powis[8207];

// gain_pow2_is[i] = 2^i
static float gain_pow2[256 + 118 + 4];

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
			} else {
				cur_ch->block_type = 0;
				cur_ch->mixed_block_flag = 0;

				for (region = 0; region < 3; ++region)
					cur_ch->table_select[region] = bs_readBits(sideinfo_stream, 5);

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

static void l3_decode_scalefactors(struct bs* const maindata_stream, struct ch_info* const cur_ch, const struct l3_sideinfo* const si, const int gr, const int ch, unsigned scf[2][39])
{
	const unsigned char slen0 = sflen_table[0][cur_ch->scalefac_compress];
	const unsigned char slen1 = sflen_table[1][cur_ch->scalefac_compress];
	int sb;

	if (cur_ch->win_switch_flag == 1 && cur_ch->block_type == 2) {
		if (cur_ch->mixed_block_flag == 1) {
			// MIXED block
			cur_ch->part2_len = slen0 * 17 + slen1 * 18;
			for (sb = 0; sb < 8; ++sb)
				scf[ch][sb] = bs_readBits(maindata_stream, slen0);
			for (sb = 9; sb < 18; ++sb)
				scf[ch][sb] = bs_readBits(maindata_stream, slen0);
			for (sb = 18; sb < 36; ++sb)
				scf[ch][sb] = bs_readBits(maindata_stream, slen1);
		} else {
			// pure SHORT block
			cur_ch->part2_len = (slen0 + slen1) * 18;
			for (sb = 0; sb < 18; ++sb)
				scf[ch][sb] = bs_readBits(maindata_stream, slen0);
			for (sb = 18; sb < 36; ++sb)
				scf[ch][sb] = bs_readBits(maindata_stream, slen1);
		}
		scf[ch][36] = scf[ch][37] = scf[ch][38] = 0;
	} else {
		// LONG types 0,1,3
		cur_ch->part2_len = 0;
		/* Scale factor bands 0-5 */
		if (!si->scfsi[ch][0] || !gr) {
			for (sb = 0; sb < 6; ++sb)
				scf[ch][sb] = bs_readBits(maindata_stream, slen0);
			cur_ch->part2_len += slen0 * 6;
		} else {
			/// Copy scalefactors from granule 0 to granule 1
			for (sb = 0; sb < 6; ++sb)
				scf[1][sb] = scf[0][sb];
		}

		/* Scale factor bands 6-10 */
		if (!si->scfsi[ch][1] || !gr) {
			for (sb = 6; sb < 11; ++sb)
				scf[ch][sb] = bs_readBits(maindata_stream, slen0);
			cur_ch->part2_len += slen0 * 5;
		} else {
			/// Copy scalefactors from granule 0 to granule 1
			for (sb = 6; sb < 11; ++sb)
				scf[1][sb] = scf[0][sb];
		}

		/* Scale factor bands 11-15 */
		if (!si->scfsi[ch][2] || !gr) {
			for (sb = 11; sb < 16; ++sb)
				scf[ch][sb] = bs_readBits(maindata_stream, slen1);
			cur_ch->part2_len += slen1 * 5;
		} else {
			/// Copy scalefactors from granule 0 to granule 1
			for (sb = 11; sb < 16; ++sb)
				scf[1][sb] = scf[0][sb];
		}

		/* Scale factor bands 16-20 */
		if (!si->scfsi[ch][3] || !gr) {
			for (sb = 16; sb < 21; ++sb)
				scf[ch][sb] = bs_readBits(maindata_stream, slen1);
			cur_ch->part2_len += slen1 * 5;
		} else {
			/// Copy scalefactors from granule 0 to granule 1
			for (sb = 16; sb < 21; ++sb)
				scf[1][sb] = scf[0][sb];
		}
		scf[ch][21] = 0;
	}
}

static void l3_huffman_decode(struct bs* const maindata_stream, struct ch_info* const cur_ch, short is[SBLIMIT * SSLIMIT])
{
	unsigned region[3], is_pos = 0;
	int part3_len = cur_ch->part2_3_len - cur_ch->part2_len;
	const struct huff_tab* htab;
	unsigned short point, bitleft, treelen, error = 0, bv = cur_ch->big_values * 2;;
	short x, y, v, w;
	char log_msg_buf[64];

	if (part3_len > 0) {
		struct bs end = { .bit_pos = maindata_stream->bit_pos + part3_len, .byte_ptr = maindata_stream->byte_ptr };
		end.byte_ptr += end.bit_pos >> 3;
		end.bit_pos &= 7;

		{
			if (cur_ch->win_switch_flag == 1) {
				region[0] = 36;
				region[1] = 576;
			} else {
				int r1 = cur_ch->region0_count + 1, r2 = r1 + cur_ch->region1_count + 1;
				if (r2 > 22) r2 = 22;
				region[0] = cur_sfb_table.index_long[r1];
				region[1] = cur_sfb_table.index_long[r2];
			}

			//if (bv > 574) {
			//	LOG_W("check_huff_stat", "bv > 574!");
			//	bv = 574;
			//}
			if (bv <= region[0])
				region[0] = region[1] = region[2] = bv;
			else if (bv <= region[1])
				region[1] = region[2] = bv;
			else
				region[2] = bv;
		}

		// 解码 bigvalues 区
		for (int r = 0; r < 3 && !error; ++r) {
			htab = ht + cur_ch->table_select[r];
			treelen = htab->treelen;
			while (is_pos < region[r]/* && part3_len >= 0*/) {
				{
					bitleft = 32;
					error = 1;
					point = 0;
					do {
						if (!(htab->table[point] & 0xff00)) {
							x = (htab->table[point] >> 4) & 0xf;
							y = htab->table[point] & 0xf;
							error = 0;
							break;
						}

						if (bs_readBit(maindata_stream)) { /* goto right-child*/
							while ((htab->table[point] & 0xff) >= 250)
								point += htab->table[point] & 0xff;
							point += htab->table[point] & 0xff;
						} else { /* goto left-child*/
							while ((htab->table[point] >> 8) >= 250)
								point += htab->table[point] >> 8;
							point += htab->table[point] >> 8;
						}
						--part3_len;
					} while (--bitleft && point < treelen/* && part3_len >= 0*/);
					if (error) {
						sprintf(log_msg_buf, "bigvalues: bitleft=%hu point=%hu treelen=%hu part3_len=%d", bitleft, point, treelen, part3_len);
						LOG_E("check_huff_stat", log_msg_buf);
						break;
					}

					if (x) {
						// get linbits
						if (htab->linbits && x == 15) {
							x += bs_readBits(maindata_stream, htab->linbits);
							part3_len -= htab->linbits;
						}
						// get sign bit
						if (bs_readBit(maindata_stream))
							x = -x;
						--part3_len;
					}
					is[is_pos++] = x;

					if (y) {
						// get linbits
						if (htab->linbits && y == 15) {
							y += bs_readBits(maindata_stream, htab->linbits);
							part3_len -= htab->linbits;
						}
						// get sign bit
						if (bs_readBit(maindata_stream))
							y = -y;
						--part3_len;
					}
					is[is_pos++] = y;
				}
			}
		}

		// 解码 count1 区
		htab = htc + cur_ch->count1table_select;
		treelen = htab->treelen;
		is_pos = bv;
		while (is_pos < 572 && part3_len > 0 && !error) {
			bitleft = 32;
			error = 1;
			point = 0;
			do {
				if (!(htab->table[point] & 0xff00)) {
					// x = (htab->table[point] >> 4) & 0xf;
					y = htab->table[point] & 0xf;
					error = 0;
					break;
				}

				if (bs_readBit(maindata_stream)) { /* goto right-child*/
					while ((htab->table[point] & 0xff) >= 250)
						point += htab->table[point] & 0xff;
					point += htab->table[point] & 0xff;
				} else { /* goto left-child*/
					while ((htab->table[point] >> 8) >= 250)
						point += htab->table[point] >> 8;
					point += htab->table[point] >> 8;
				}
				--part3_len;
			} while (--bitleft && point < treelen/* && part3_len >= 0*/);
			if (error) {
				if (part3_len < -1) {
					sprintf(log_msg_buf, "count1: bitleft=%hu point=%hu treelen=%hu part3_len=%d", bitleft, point, treelen, part3_len);
					LOG_E("check_huff_stat", log_msg_buf);
				}
				break;
			}

			x = (y >> 3) & 0x1;
			v = (y >> 2) & 0x1;
			w = (y >> 1) & 0x1;
			y &= 0x1;

			if (x) {
				if (bs_readBit(maindata_stream))
					x = -x;
				if (--part3_len < 0) {
					LOG_E("check_huff_stat", "count1: x");
					//break;
				}
			}

			if (v) {
				if (bs_readBit(maindata_stream))
					v = -v;
				if (--part3_len < 0) {
					LOG_E("check_huff_stat", "count1: v");
					//break;
				}
			}

			if (w) {
				if (bs_readBit(maindata_stream))
					w = -w;
				if (--part3_len < 0) {
					LOG_E("check_huff_stat", "count1: w");
					//break;
				}
			}

			if (y) {
				if (bs_readBit(maindata_stream))
					y = -y;
				if (--part3_len < 0) {
					LOG_E("check_huff_stat", "count1: y");
					//break;
				}
			}

			is[is_pos++] = x;
			if (is_pos >= 576)
				break;
			is[is_pos++] = v;
			if (is_pos >= 576)
				break;
			is[is_pos++] = w;
			if (is_pos >= 576)
				break;
			is[is_pos++] = y;
		}

		if (part3_len < 0) {
			if (part3_len + 1 < 0)
				is_pos -= 4;
			LOG_E("check_huff_stat", "part3_len < 0");
		} else if (part3_len > 0) {
			LOG_E("check_huff_stat", "part3_len > 0");
		}

		maindata_stream->byte_ptr = end.byte_ptr;
		maindata_stream->bit_pos = end.bit_pos;
	}

	cur_ch->nonzero_len = is_pos;

	while (is_pos < 576)
		is[is_pos++] = 0;
}

//static void l3_requantize_long(const struct ch_info* const cur_ch, const int sfb, const int pos, const short is[SBLIMIT * SSLIMIT], float xr[SBLIMIT * SSLIMIT])
//{
//	const double sf_mult = cur_ch->scalefac_scale != 0 ? 1.0 : 0.5;
//	float tmp1, tmp2, tmp3;
//
//	if (sfb < 21)
//		tmp1 = (float)pow(2.0, -(sf_mult * ((double)cur_ch->scalefac_l[sfb] + pretab[cur_ch->preflag][sfb])));
//	else tmp1 = 1.0f;
//
//	tmp2 = (float)pow(2.0, 0.25 * (cur_ch->global_gain - 210.0));
//
//	if (is[pos] < 0)
//		tmp3 = -gain_powis[-is[pos]];
//	else tmp3 = gain_powis[is[pos]];
//
//	xr[pos] = tmp1 * tmp2 * tmp3;
//}
//
//static void l3_requantize_short(const struct ch_info* const cur_ch, const int sfb, const int window, const int pos, const short is[SBLIMIT * SSLIMIT], float xr[SBLIMIT * SSLIMIT])
//{
//	const double sf_mult = cur_ch->scalefac_scale != 0 ? 1.0 : 0.5;
//	float tmp1, tmp2, tmp3;
//
//	if (sfb < 12)
//		tmp1 = (float)pow(2.0, -(sf_mult * cur_ch->scalefac_s[sfb * 3 + window]));
//	else tmp1 = 1.0f;
//
//	tmp2 = (float)pow(2.0, 0.25 * (cur_ch->global_gain - 210.0 - cur_ch->subblock_gain[window] * 8.0));
//
//	if (is[pos] < 0)
//		tmp3 = -gain_powis[-is[pos]];
//	else tmp3 = gain_powis[is[pos]];
//
//	xr[pos] = tmp1 * tmp2 * tmp3;
//}
//
////static void l3_reorder(const struct ch_info* const cur_ch, const short is[SBLIMIT * SSLIMIT], float xr[SBLIMIT * SSLIMIT])
////{
////	int sfb = 0, next_sfb, is_pos = 0, window, width, i;
////
////
////}

static void l3_requantize(const struct ch_info* cur_ch, const struct mpeg_frame* frame, const short is[SBLIMIT * SSLIMIT], const unsigned scf[39], float xr[SBLIMIT * SSLIMIT])
{
	unsigned is_pos = 0, pow2i = 255 - cur_ch->global_gain, sfb = 0, window, width, xri_start = 0, xri = 0, bi, shift = cur_ch->scalefac_scale + 1;
	const unsigned char* pre = pretab[cur_ch->preflag];

	if (frame->is_MS)
		pow2i += 2;

	if (cur_ch->nonzero_len) {
		if (cur_ch->win_switch_flag == 1 && cur_ch->block_type == 2) {
			if (cur_ch->mixed_block_flag == 1) { /* MIXED BLOCk*/
				for (; sfb < 8; ++sfb, ++scf, ++pre) {
					width = cur_sfb_table.width_long[sfb];
					for (bi = 0; bi < width; ++bi, ++is_pos) {
						if (is[is_pos] < 0) {
							xr[is_pos] = -gain_pow2[pow2i + ((*scf + *pre) << shift)] * gain_powis[-is[is_pos]];
						} else if (is[is_pos] > 0) {
							xr[is_pos] = gain_pow2[pow2i + ((*scf + *pre) << shift)] * gain_powis[is[is_pos]];
						} else
							xr[is_pos] = 0.0f;
					}
				}
				++scf;
				xri_start = 36;
				sfb = 3;
			}
			/* pure SHORT BLOCK */
			for (; is_pos < cur_ch->nonzero_len; ++sfb) {
				width = cur_sfb_table.width_short[sfb];
				for (window = 0; window < 3; ++window, ++scf) {
					xri = xri_start + window;
					for (bi = 0; bi < width; ++bi, ++is_pos) {
						if (is[is_pos] < 0) {
							xr[xri] = -gain_pow2[pow2i + cur_ch->subblock_gain[window] * 8 + (*scf << shift)] * gain_powis[-is[is_pos]];
						} else if (is[is_pos] > 0) {
							xr[xri] = gain_pow2[pow2i + cur_ch->subblock_gain[window] * 8 + (*scf << shift)] * gain_powis[is[is_pos]];
						} else
							xr[xri] = 0.0f;
						xri += 3;
					}
				}
				xri_start = xri - 2;
			}
		} else { /* pure LONG BLOCK */
			for (; is_pos < cur_ch->nonzero_len; ++sfb, ++scf, ++pre) {
				width = cur_sfb_table.width_long[sfb];
				for (bi = 0; bi < width; ++bi, ++is_pos) {
					if (is[is_pos] < 0) {
						xr[is_pos] = -gain_pow2[pow2i + ((*scf + *pre) << shift)] * gain_powis[-is[is_pos]];
					} else if (is[is_pos] > 0) {
						xr[is_pos] = gain_pow2[pow2i + ((*scf + *pre) << shift)] * gain_powis[is[is_pos]];
					} else
						xr[is_pos] = 0.0f;
				}
			}
		}
	}

	// 不逆量化0值区,置0.
	while (is_pos < 576)
		xr[is_pos++] = 0.0f;
}

static void l3_do_ms_stereo(const unsigned max, float xr[2][SBLIMIT * SSLIMIT])
{
	for (unsigned i = 0; i < max; ++i) {
		const float v0 = xr[0][i] + xr[1][i];
		const float v1 = xr[0][i] - xr[1][i];
		xr[0][i] = v0;
		xr[1][i] = v1;
	}
}

static void l3_do_intesity_stereo(struct gr_info* cur_gr, const unsigned scf[39], float xr[2][SBLIMIT * SSLIMIT])
{
	int sfb, is_possb, width, sfb_start, sfb_stop, i, window;
	double is_ratio_l, is_ratio_r;

	if (cur_gr->ch[0].mixed_block_flag != cur_gr->ch[1].mixed_block_flag || cur_gr->ch[0].block_type != cur_gr->ch[1].block_type) {
		LOG_W("check_stereo", "bad stereo!");
		return;
	}

	if (cur_gr->ch[0].win_switch_flag == 1 && cur_gr->ch[0].block_type == 2) {
		// MPEG-1, short block/mixed block
		if (cur_gr->ch[0].mixed_block_flag == 1) {
			for (sfb = 0; sfb < 8; ++sfb) {
				if (cur_sfb_table.index_long[sfb] < cur_gr->ch[1].nonzero_len)
					continue;
				if ((is_possb = scf[sfb]) == 7)
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
					xr[0][i] *= (float)is_ratio_l;
					xr[1][i] *= (float)is_ratio_r;
				}
			}

			for (sfb = 3; sfb < 12; ++sfb) {
				if (cur_sfb_table.index_short[sfb] < cur_gr->ch[1].nonzero_len)
					continue;
				width = cur_sfb_table.width_short[sfb];
				for (window = 0; window < 3; ++window) {
					if ((is_possb = scf[sfb * 3 + window]) == 7)
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
						xr[0][i] *= (float)is_ratio_l;
						xr[1][i] *= (float)is_ratio_r;
					}
				}
			}
		} else {
			for (sfb = 0; sfb < 12; ++sfb) {
				if (cur_sfb_table.index_short[sfb] < cur_gr->ch[1].nonzero_len)
					continue;
				width = cur_sfb_table.width_short[sfb];
				for (window = 0; window < 3; ++window) {
					if ((is_possb = scf[sfb * 3 + window]) == 7)
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
						xr[0][i] *= (float)is_ratio_l;
						xr[1][i] *= (float)is_ratio_r;
					}
				}
			}
		}
	} else {
		// MPEG-1, long block
		for (sfb = 0; sfb < 21; ++sfb) {
			if (cur_sfb_table.index_long[sfb] < cur_gr->ch[1].nonzero_len)
				continue;
			if ((is_possb = scf[sfb]) == 7)
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
				xr[0][i] *= (float)is_ratio_l;
				xr[1][i] *= (float)is_ratio_r;
			}
		}
	}
}

static void l3_antialias(const struct ch_info* cur_ch, float xr[SBLIMIT * SSLIMIT])
{
	int sblimit, sb, i;

	if (cur_ch->win_switch_flag == 1 && cur_ch->block_type == 2) {
		if (cur_ch->mixed_block_flag == 0)
			return;
		sblimit = SSLIMIT;
	} else
		sblimit = ((cur_ch->nonzero_len + SBLIMIT - 1) / SBLIMIT) * SSLIMIT;

	for (sb = 0; sb < sblimit; sb += SSLIMIT) {
		for (i = 0; i < 8; ++i) {
			const float lb = xr[sb + 17 - i] * cs[i] - xr[sb + 18 + i] * ca[i];
			const float ub = xr[sb + 18 + i] * cs[i] - xr[sb + 17 - i] * ca[i];
			xr[sb + 17 - i] = lb;
			xr[sb + 18 + i] = ub;
		}
	}
}

static void imdct12(const float xr[SSLIMIT], float rawout[36])
{
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 12; ++j) {
			float sum = 0.0f;
			for (int k = 0; k < 6; ++k) {
				sum += xr[i + 3 * k] * imdct_s[k][j];
			}
			rawout[6 * i + j + 6] = sum * imdct_window[2][j];
		}
	}
}

static void imdct36(const float xr[SSLIMIT], float rawout[36], unsigned char block_type)
{
	for (int i = 0; i < 36; ++i) {
		float sum = 0.0f;
		for (int j = 0; j < SSLIMIT; ++j) {
			sum += xr[j] * imdct_l[j][i];
		}
		rawout[i] = sum * imdct_window[block_type][i];
	}
}

static void l3_hybrid(const struct ch_info* cur_ch, const int ch, float xr[SBLIMIT * SSLIMIT])
{
	float rawout[36];
	unsigned off;

	for (off = 0; off < cur_ch->nonzero_len; off += SSLIMIT) {
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
	for (; off < 576; ++off) {
		xr[off] = overlapp[ch][off];
		overlapp[ch][off] = 0.0f;
	}
}

void l3_init(const struct mpeg_header* const header)
{
	cur_sfb_table.index_long = __sfb_index_long[header->sampling_frequency];
	cur_sfb_table.index_short = __sfb_index_short[header->sampling_frequency];
	cur_sfb_table.width_long = __sfb_width_long[header->sampling_frequency];
	cur_sfb_table.width_short = __sfb_width_short[header->sampling_frequency];

	int i, j;
	for (i = 0; i < 378; ++i) {
		gain_pow2[i] = (float)pow(2.0, -0.25 * ((double)i - 45.0));
	}

	for (i = 0; i < 8207; ++i) {
		gain_powis[i] = (float)pow((double)i, 4.0 / 3.0);
	}

	//for (i = -256; i < 118 + 4; ++i)
	//	gain_pow2_is[i + 256] = (float)pow(2.0, (i + 210.0) * -1.0 / 4.0);

	for (i = 0; i < 8; ++i) {
		double sq = sqrt(1.0 + __Ci[i] * __Ci[i]);
		cs[i] = (float)(1.0 / sq);
		ca[i] = (float)(__Ci[i] / sq);
	}

	{
		for (i = 0; i < 6; ++i) {
			imdct_window[0][i] = (float)sin(M_PI * (2 * i + 1) / 72);
			imdct_window[1][i] = imdct_window[0][i];
			imdct_window[2][i] = (float)sin(M_PI * (2 * i + 1) / 24);
			// imdct_window[3][i] = 0.0f;
		}

		for (; i < 12; ++i) {
			imdct_window[0][i] = (float)sin(M_PI * (2 * i + 1) / 72);
			imdct_window[1][i] = imdct_window[0][i];
			imdct_window[2][i] = (float)sin(M_PI * (2 * i + 1) / 24);
			imdct_window[3][i] = (float)sin(M_PI * (2 * i - 15) / 24);
		}

		for (; i < 18; ++i) {
			imdct_window[0][i] = (float)sin(M_PI * (2 * i + 1) / 72);
			imdct_window[1][i] = imdct_window[0][i];
			imdct_window[3][i] = 1.0f;
		}

		for (; i < 24; ++i) {
			imdct_window[0][i] = (float)sin(M_PI * (2 * i + 1) / 72);
			imdct_window[1][i] = 1.0f;
			imdct_window[3][i] = imdct_window[0][i];
		}

		for (; i < 30; ++i) {
			imdct_window[0][i] = (float)sin(M_PI * (2 * i + 1) / 72);
			imdct_window[1][i] = (float)sin(M_PI * (2 * i - 35) / 24);
			imdct_window[3][i] = imdct_window[0][i];
		}

		for (; i < 36; ++i) {
			imdct_window[0][i] = (float)sin(M_PI * (2 * i + 1) / 72);
			imdct_window[3][i] = imdct_window[0][i];
		}
	}

	for (i = 0; i < 6; ++i) {
		for (j = 0; j < 12; ++j) {
			imdct_s[i][j] = (float)cos(M_PI * ((2 * j + 7) * (2 * i + 1)) / 24);
		}
	}

	for (i = 0; i < 18; ++i) {
		for (j = 0; j < 36; ++j) {
			imdct_l[i][j] = (float)cos(M_PI * ((2 * j + 19) * (2 * i + 1)) / 72);
		}
	}

	// static float is_coef[] = { 0.0, 0.211324865, 0.366025404, 0.5, 0.633974596, 0.788675135, 1.0 };
	//float is_ratio;
	//for (i = 0; i < 7; ++i) {
	//	is_ratio = tan(i * PI / 12.0);
	//	is_table[i] = (float)(is_ratio / (1.0 + is_ratio));
	//}
	for (i = 0; i < 6; ++i) {
		is_ratio[i] = (float)tan(i * M_PI / 12);
	}

	init_synthesis_tabs();
}

static short is[SBLIMIT * SSLIMIT];
static float xr[2][SBLIMIT * SSLIMIT];
int l3_decode_samples(struct decoder_handle* handle, unsigned frame_count)
{
	const struct mpeg_frame* const cur_frame = &handle->cur_frame;
	struct bs* const file_stream = handle->file_stream;
	struct bs* const sideinfo_stream = handle->sideinfo_stream;
	struct bs* const maindata_stream = handle->maindata_stream;
	struct l3_sideinfo sideinfo;
	/*
	* short: 36, mixed: 8 + 27, long: 21
	*/
	unsigned scalefac[2][39];	// scalefac[ch][sfb], scalefactor band(缩放因子带)
	int gr, ch;
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
		return -1;
	}

	maindata_stream->byte_ptr = maindata_stream->end_ptr - sideinfo.main_data_begin;
	maindata_stream->bit_pos = 0;

#if 0
	int discard = bs_Avaliable(maindata_stream) - sideinfo.main_data_begin;
	bs_skipBytes(maindata_stream, discard);
#endif

	if (bs_Append(maindata_stream, sideinfo_stream->byte_ptr, 0, cur_frame->maindata_size) != cur_frame->maindata_size) {
		sprintf(log_msg_buf, "frame#%u maindata_stream overflow!", frame_count);
		LOG_E("bs_Append(maindata_stream)", log_msg_buf);
		return 1;
	}

	for (gr = 0; gr < 2; ++gr) {
		struct gr_info* cur_gr = &sideinfo.gr[gr];
		l3_decode_scalefactors(maindata_stream, &cur_gr->ch[0], &sideinfo, gr, 0, scalefac);
		l3_huffman_decode(maindata_stream, &cur_gr->ch[0], is);
		l3_requantize(&cur_gr->ch[0], cur_frame, is, scalefac[0], xr[0]);

		if (cur_frame->nch == 2) {
			l3_decode_scalefactors(maindata_stream, &cur_gr->ch[1], &sideinfo, gr, 1, scalefac);
			l3_huffman_decode(maindata_stream, &cur_gr->ch[1], is);
			l3_requantize(&cur_gr->ch[1], cur_frame, is, scalefac[1], xr[1]);

			if (cur_frame->is_MS || cur_frame->is_Intensity) {
				if (cur_gr->ch[0].nonzero_len > cur_gr->ch[1].nonzero_len)
					cur_gr->ch[1].nonzero_len = cur_gr->ch[0].nonzero_len;
				else cur_gr->ch[0].nonzero_len = cur_gr->ch[1].nonzero_len;

				if (cur_frame->is_MS);
				l3_do_ms_stereo(cur_gr->ch[0].nonzero_len, xr);
				if (cur_frame->is_Intensity) {
					LOG_W("chech_stereo", "intesity_stereo not supported!");
					// l3_do_intesity_stereo(cur_gr, scalefac[0], xr);
				}
			}
		}

		{
			int sb, ss, i;
			float s[32];
			for (ch = 0; ch < cur_frame->nch; ++ch) {
				l3_antialias(&sideinfo.gr[gr].ch[ch], xr[ch]);
				l3_hybrid(&sideinfo.gr[gr].ch[ch], ch, xr[ch]);

				/* frequency inversion */
				for (sb = 1; sb < 32; sb += 2) {
					for (i = 1; i < 18; i += 2) {
						xr[ch][sb * 18 + i] = -xr[ch][sb * 18 + i];
					}
				}

				for (ss = 0; ss < SSLIMIT; ++ss) {
					for (i = 0; i < 32; i++) {
						s[i] = xr[ch][i * 18 + ss];
					}
					/* polyphase subband synthesis */
					synthesis_subband_filter(s, ch, cur_frame->nch, handle->pcm.pcm_buf, handle->pcm.write_off + ch);
				}
			}
		}
	}

	return 0;
}
