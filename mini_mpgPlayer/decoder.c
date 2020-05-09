#include "decoder.h"
#include "layer3.h"
#include "audio.h"
#include "tag.h"
#include <stdlib.h>

struct decoder_handle* decoder_Init(const char* const mp3_file_name, const int output_flags, const char* const wav_file_name)
{
	struct decoder_handle* handle = NULL;

	do {
		if (!mp3_file_name)
			break;
		if (!(handle = calloc(1, sizeof(struct decoder_handle))))
			break;

		handle->file_stream = bs_Init(4096, mp3_file_name);
		handle->sideinfo_stream = bs_Init(0, NULL);
		handle->maindata_stream = bs_Init(4096, NULL);
		if (!handle->file_stream || !handle->sideinfo_stream || !handle->maindata_stream)
			break;

		if (output_flags & OUTPUT_AUDIO)
			handle->output_flags |= OUTPUT_AUDIO;
		if (wav_file_name && output_flags & OUTPUT_FILE) {
			handle->output_flags |= OUTPUT_FILE;
			if (!(handle->wav_ptr = fopen(wav_file_name, "wb")))
				break;
		}

		return handle;
	} while (0);

	decoder_Release(&handle);
	return NULL;
}

void decoder_Release(const struct decoder_handle** const handle)
{
	if (handle && *handle) {
		if ((*handle)->output_flags & OUTPUT_AUDIO)
			audio_close();
		if (/*(*handle)->output_flags & OUTPUT_FILE && */(*handle)->wav_ptr)
			fclose((*handle)->wav_ptr);
		if ((*handle)->sideinfo_stream)
			(*handle)->sideinfo_stream->bit_buf = NULL;
		bs_Release(&(*handle)->file_stream);
		bs_Release(&(*handle)->sideinfo_stream);
		bs_Release(&(*handle)->maindata_stream);
		free(*handle);
		*handle = NULL;
	}
}

static const char* const version_str[] = { "2.5", "Reserved", "2.0", "1.0" };
static const char* const layer_str[] = { "Reserved", "III", "II", "I" };
static const char* const mode_str[] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Mono" };
static void print_header_info(const struct mpeg_frame* const frame)
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
	printf(" %ubytes\n\n", frame->frame_size);
}

unsigned decoder_Run(struct decoder_handle* const handle)
{
	struct mpeg_frame* const cur_frame = &handle->cur_frame;
	struct pcm_stream* const pcm_out = &handle->pcm;
	unsigned frame_count = 0;
	int stat;
	char log_msg_buf[64];

	decode_id3v1(handle->file_stream);

	unsigned id3v2_size;
	while (decode_id3v2(handle->file_stream, &id3v2_size) == 0) {
		fseek(handle->file_stream->file_ptr, id3v2_size, SEEK_CUR);
		handle->file_stream->end_ptr = handle->file_stream->bit_buf;
	}

	if (decode_next_frame(cur_frame, handle->file_stream) == -1) {
		LOG_E("decode_next_frame", "can't find the first frame!");
		return 0;
	}

	print_header_info(cur_frame);

	if (cur_frame->header.version != VERSION_10 || cur_frame->header.layer != LAYER_3) {
		sprintf(log_msg_buf, "not support the [MPEG %s Layer %s] now!", version_str[cur_frame->header.version], layer_str[cur_frame->header.layer]);
		LOG_E("check_support", log_msg_buf);
		return 0;
	}

	if (cur_frame->freeformat) {
		LOG_E("check_support", "not support the [freeformat bitrate] now!");
		return 0;
	}

	l3_init(&cur_frame->header);

	if (handle->output_flags & OUTPUT_AUDIO) {
		if (audio_open(cur_frame->nch, cur_frame->samplingrate) == -1) {
			LOG_E("audio_open", "init the audio output device failed!");
			return 0;
		}
	}

	pcm_out->write_off[0] = pcm_out->read_off = 0;
	pcm_out->write_off[1] = 2;
	pcm_out->audio_buf_size = cur_frame->pcm_size * 4;
	pcm_out->pcm_buf_size = pcm_out->audio_buf_size * 4;
	if (!(pcm_out->pcm_buf = malloc(pcm_out->pcm_buf_size))) {
		LOG_E("malloc(pcm_buf)", "init the pcm_stream failed!");
		return 0;
	}

	if (get_vbr_tag(handle->file_stream, cur_frame) == 0) {
		bs_skipBytes(handle->file_stream, cur_frame->sideinfo_size + cur_frame->maindata_size);
		if (decode_next_frame(cur_frame, handle->file_stream) == -1) {
			LOG_E("decode_next_frame", "can't find the first frame!");
			return 0;
		}
		++frame_count;
	}

	do {
		++frame_count;
		print_header_info(cur_frame);

		if ((stat = l3_decode_samples(handle, frame_count)) == -1)
			break;

		bs_skipBytes(handle->file_stream, cur_frame->sideinfo_size + cur_frame->maindata_size);
		if (stat == 1)
			continue;

		if (pcm_out->write_off[0] >= pcm_out->audio_buf_size + pcm_out->read_off && (cur_frame->nch != 2 || pcm_out->write_off[1] >= pcm_out->audio_buf_size + pcm_out->read_off)) {
			if (handle->output_flags & OUTPUT_AUDIO && !play_samples(pcm_out->pcm_buf + pcm_out->read_off, pcm_out->audio_buf_size)) {
				sprintf(log_msg_buf, "frame#%u play failed!", frame_count);
				LOG_E("play_samples", log_msg_buf);
			}
			if (handle->output_flags & OUTPUT_FILE && fwrite(pcm_out->pcm_buf + pcm_out->read_off, 1, pcm_out->audio_buf_size, handle->wav_ptr) != pcm_out->audio_buf_size) {
				sprintf(log_msg_buf, "frame#%u write failed!", frame_count);
				LOG_E("fwrite", log_msg_buf);
			}
			if ((pcm_out->read_off += pcm_out->audio_buf_size) >= pcm_out->pcm_buf_size) {
				pcm_out->write_off[0] = pcm_out->read_off = 0;
				pcm_out->write_off[1] = 2;
			}
		}
	} while (decode_next_frame(cur_frame, handle->file_stream) != -1);

	return frame_count;
}
