#include "frame.h"
#include "layer3.h"
#include "audio_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <Windows.h>

#define VBR_TAG_INFO	0x6f666e49U	// CBR (Constant Bit Rate).
#define VBR_TAG_XING	0x676e6958U	// VBR/ABR (Variable Bit Rate/Average Bit Rate).

int byte2int(const struct bitstream* stream, int off)
{
	int i = stream->byte_ptr[off];
	i <<= 8;
	i |= stream->byte_ptr[off + 1];
	i <<= 8;
	i |= stream->byte_ptr[off + 2];
	i <<= 8;
	i |= stream->byte_ptr[off + 3];
	return i;
}

int get_vbr_tag(const struct bitstream* stream, const struct mpeg_frame* frame)
{
	if (frame->header.version != VERSION_10 || frame->header.layer != LAYER_3)
		return -1;

	unsigned off = frame->sideinfo_size;
	unsigned tag_magic = *(unsigned*)(stream->byte_ptr + off);

	if (tag_magic == VBR_TAG_INFO)
		puts("Info - CBR (Constant Bit Rate)");
	else if (tag_magic == VBR_TAG_XING)
		puts("Xing - VBR/ABR (Variable Bit Rate/Average Bit Rate)");
	else return -1;
	off += 4;

	unsigned char flags = stream->byte_ptr[off + 3];
	off += 4;

	double duration = 0;
	if (flags & 0x1) {	// Number of Frames
		int frames = byte2int(stream, off);
		duration = frames * (frame->lsf ? 576.0 : 1152.0) / frame->samplingrate;
		printf("Number of Frames: %d --> %.2lfsecs\n", frames, duration);
		off += 4;
	}

	if (flags & 0x2) { // Size in Bytes
		int size = byte2int(stream, off);
		duration = size / (16.0 * frame->samplingrate * frame->nch / 8);
		printf("Size in Bytes: %dbytes --> %.2lfsecs\n", size, duration);
		off += 4;
	}

	if (flags & 0x4) { // TOC Data
		puts("TOC Data: Existent");
		off += 100;
	}

	if (flags & 0x8) { // VBR Scale
		printf("VBR Scale: %d\n", byte2int(stream, off));
		off += 4;
	}

	char encoder_str[16 + 1];
	unsigned len = 0;
	while (len < 16 && (isalnum(stream->byte_ptr[off + len]) || stream->byte_ptr[off + len] == '.' || stream->byte_ptr[off + len] == ' ')) {
		encoder_str[len] = stream->byte_ptr[off + len];
		++len;
	}
	if (len >= 4) {
		encoder_str[len] = 0;
		printf("encoder: %s\n", encoder_str);
	}

	//tag_magic = *(unsigned*)(stream->byte_ptr + off);
	//if (tag_magic == 'EMAL') {	// LAME
	//	// LAME<major>.<minor><release>
	//	printf("encoder: LAME %c.%c%c %c\n", stream->byte_ptr[4], stream->byte_ptr[6], stream->byte_ptr[7], stream->byte_ptr[8]);
	//} else if (tag_magic == 'fvaL') {	// Lavf
	//	printf("encoder: Lavf %c%c.%c.%c%c%c\n", stream->byte_ptr[4], stream->byte_ptr[5], stream->byte_ptr[7], stream->byte_ptr[9], stream->byte_ptr[10], stream->byte_ptr[11]);
	//} else if (tag_magic == 'cvaL') {	// Lavc
	//	printf("encoder: Lavc %c%c.%c%c\n", stream->byte_ptr[4], stream->byte_ptr[5], stream->byte_ptr[7], stream->byte_ptr[8]);
	//}
	putchar('\n');
	return 0;
}

void safe_print(const char* const str)
{
	int i = 0;
	while (i < 30) {
		putchar(str[i++]);
	}
	putchar('\n');
}

void decode_id3v1(struct bitstream* stream)
{
	if (fseek(stream->file_stream, -128, SEEK_END))
		return;

	if (bs_prefect(stream, 128) == 128) {
		if (stream->byte_ptr[0] == 'T' && stream->byte_ptr[1] == 'A' && stream->byte_ptr[2] == 'G') {
			printf("ID3 1.%d\n", stream->byte_ptr[126] && stream->byte_ptr[126] != ' ');
			if (stream->byte_ptr[3] && stream->byte_ptr[3] != ' ') {
				printf("title: ");
				safe_print(stream->byte_ptr + 3);
			}
			if (stream->byte_ptr[33] && stream->byte_ptr[33] != ' ') {
				printf("artist: ");
				safe_print(stream->byte_ptr + 33);
			}
			if (stream->byte_ptr[63] && stream->byte_ptr[63] != ' ') {
				printf("album: ");
				safe_print(stream->byte_ptr + 63);
			}
			if (stream->byte_ptr[97] && stream->byte_ptr[97] != ' ') {
				printf("comment: ");
				safe_print(stream->byte_ptr + 97);
			}
			if (stream->byte_ptr[93] && stream->byte_ptr[93] != ' ') {
				printf("year: %d%d%d%d\n", stream->byte_ptr[93], stream->byte_ptr[94], stream->byte_ptr[95], stream->byte_ptr[96]);
			}
			if (stream->byte_ptr[126] && stream->byte_ptr[126] != ' ')
				printf("track: %d\n", stream->byte_ptr[126]);
			printf("genre: %d\n", stream->byte_ptr[127]);
			putchar('\n');
		}
	}

	fseek(stream->file_stream, 0, SEEK_SET);
	stream->end_ptr = stream->bit_reservoir;
}

int decode_id3v2(struct bitstream* stream, unsigned* size)
{
	if (bs_prefect(stream, 10) != 10)
		return -1;

	if (stream->byte_ptr[0] != 'I' || stream->byte_ptr[1] != 'D' || stream->byte_ptr[2] != '3')
		return -1;

	*size = stream->byte_ptr[6] & 0x7f;
	*size <<= 7;
	*size |= stream->byte_ptr[7] & 0x7f;
	*size <<= 7;
	*size |= stream->byte_ptr[8] & 0x7f;
	*size <<= 7;
	*size |= stream->byte_ptr[9] & 0x7f;

	printf("ID3 2.%d%d\n" \
		"flag: 0x%x\n" \
		"size: %ubytes\n\n",
		stream->byte_ptr[3], stream->byte_ptr[4], stream->byte_ptr[5], *size + 10);

	return 0;
}

static const char* const version_str[] = { "2.5", "Reserved", "2.0", "1.0" };
static const char* const layer_str[] = { "Reserved", "III", "II", "I" };
static const char* const mode_str[] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Mono" };
void print_header_info(const struct mpeg_frame* frame)
{
	printf("MPEG-%s Layer %s %ukbps %uHz %s", version_str[frame->header.version], layer_str[frame->header.layer], frame->bitrate, frame->samplingrate, mode_str[frame->header.mode]);
	if (frame->header.layer == LAYER_3 && frame->header.mode == MODE_JointStereo) {
		if (frame->is_MS && !frame->is_Intensity)
			printf(" (M/S)");
		else if (!frame->is_MS && frame->is_Intensity)
			printf(" (I/S)");
		else if (frame->is_MS && frame->is_Intensity)
			printf(" (M/S & I/S)");
	}
	printf(" %ubytes\n", frame->frame_size);
}

int main(int argc, char** argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s [*.mp3]\n", *argv);
		return 0;
	}

	struct bitstream* decode_stream = bitstream_init(0, NULL);
	struct bitstream* maindata_stream = bitstream_init(4096, NULL);
	struct bitstream* mp3_file = bitstream_init(4096, argv[1]);
	if (!mp3_file || !decode_stream || !maindata_stream) {
		fprintf(stderr, "[E] %s:%d %s::bitstream_init failed!\n", __FILE__, __LINE__, __func__);
		bitstream_release(&mp3_file);
		bitstream_release(&decode_stream);
		bitstream_release(&maindata_stream);
		return -1;
	}

	decode_id3v1(mp3_file);

	unsigned id3v2_size;
	while (decode_id3v2(mp3_file, &id3v2_size) == 0) {
		fseek(mp3_file->file_stream, id3v2_size, SEEK_CUR);
		mp3_file->end_ptr = mp3_file->bit_reservoir;
	}

	int ret = -1, stat;
	unsigned frame_count = 0;
	struct mpeg_frame cur_frame;
	// bool quitting = false;
	clock_t s, e;

	do {
		s = clock();

		if (decode_next_frame(&cur_frame, mp3_file) == -1) {
			fprintf(stderr, "[E] can't find the first frame!\n");
			break;
		}

		if (cur_frame.header.version != VERSION_10 || cur_frame.header.layer != LAYER_3) {
			fprintf(stderr, "[E] not support the [MPEG %s Layer %s] now!\n", version_str[cur_frame.header.version], layer_str[cur_frame.header.layer]);
			break;
		}

		if (cur_frame.freeformat) {
			fprintf(stderr, "[E] not support the [freeformat] now!\n");
			break;
		}

		layer3_init(&cur_frame.header);

		stat = get_vbr_tag(mp3_file, &cur_frame);

		FILE* wavFile = fopen("test1.wav", "wb");
		// fwrite("RIFF____WAVEfmt ____**##@@@@!!!!$$%%data____", 1, 44, wavFile);

		if (!audio_open(cur_frame.nch, cur_frame.samplingrate)) {
			fprintf(stderr, "[E] init the audio output device failed!\n");
			break;
		}

		unsigned read_ptr = 0;
		unsigned write_ptr[2] = { 0, 2 };
		unsigned pcm_size = (cur_frame.lsf ? 2304 : 4608) >> (cur_frame.nch & 1);
		unsigned audio_buf_size = pcm_size * 4;
		unsigned pcm_stream_size = audio_buf_size * 6;
		unsigned char* pcm_stream = malloc(pcm_stream_size);
		if (!pcm_stream) {
			fprintf(stderr, "[E] init the pcm_stream failed!\n");
			break;
		}

		if (stat == 0) {
			mp3_file->byte_ptr += cur_frame.sideinfo_size + cur_frame.maindata_size;
			if (decode_next_frame(&cur_frame, mp3_file) == -1) {
				ret = 0;
				break;
			}
			++frame_count;
		}

		do {
			++frame_count;
			ret = 0;
			// print_header_info(&cur_frame);

			if ((stat = l3_decode_samples(mp3_file, decode_stream, maindata_stream, &cur_frame, pcm_stream, write_ptr, frame_count)) == -1) {
				ret = -1;
				break;
			}
			mp3_file->byte_ptr += cur_frame.sideinfo_size + cur_frame.maindata_size;
			if (stat == 1)
				continue;

			if (write_ptr[0] >= audio_buf_size + read_ptr && (cur_frame.nch != 2 || write_ptr[1] >= audio_buf_size + read_ptr)) {
				if (!play_samples(pcm_stream + read_ptr, audio_buf_size)) {
					fprintf(stderr, "[E] frame #%u play failed!\n", frame_count);
				}
				if (fwrite(pcm_stream + read_ptr, 1, audio_buf_size, wavFile) != audio_buf_size) {
					fprintf(stderr, "[E] frame #%u write failed!\n", frame_count);
				}
				if ((read_ptr += audio_buf_size) >= pcm_stream_size) {
					write_ptr[0] = read_ptr = 0;
					write_ptr[1] = 2;
				}
			}
		} while (decode_next_frame(&cur_frame, mp3_file) != -1);
		audio_close();
		e = clock();
		fclose(wavFile);
		free(pcm_stream);
		decode_stream->bit_reservoir = NULL;
	} while (false);

	if (frame_count) {
		printf("\ntime: %.3lfsecs", ((double)e - s) / CLOCKS_PER_SEC);
		printf("\nframe count: %u\n", frame_count);
	}

	bitstream_release(&mp3_file);
	bitstream_release(&decode_stream);
	bitstream_release(&maindata_stream);
	getchar();
	return ret;
}
