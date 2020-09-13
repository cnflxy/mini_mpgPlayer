#include "decoder.h"
#include <stdio.h>
#include <time.h>

int main(int argc, char** argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s [*.mp3]\n", *argv);
		return -1;
	}

	printf("Input: \"%s\"\n\n", argv[1]);

	struct decoder_handle* decoder = decoder_Init(argv[1], OUTPUT_AUDIO, NULL);
	if (!decoder) {
		LOG_E("decoder_Init", "failed!");
		return -1;
	}

	clock_t s = clock(), e;
	uint32_t frame_count = decoder_Run(decoder);
	decoder_Release(&decoder);
	e = clock();
	if (frame_count) {
		printf("\ntime: %.2lfsecs", ((double)e - s) / CLOCKS_PER_SEC);
		printf("\nframe count: %u\n", frame_count);
	}

	(void)getchar();

	return 0;
}
