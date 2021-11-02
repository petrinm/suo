
#include "suo.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define SUO_COLORS

struct frame* suo_frame_new(unsigned int data_len) {
	struct frame* frame = calloc(1, sizeof(struct frame));
	frame->metadata = calloc(MAX_METADATA, sizeof(struct metadata));
	frame->data = calloc(1, data_len);
	frame->data_len = 0;
	//frame->data_alloc_len = data_len;
	return frame;
}

void suo_frame_clear(struct frame* frame) {
	assert(frame != NULL);
	memset(&frame->hdr, 0, sizeof(struct frame_header));
	memset(frame->metadata, 0, MAX_METADATA * sizeof(struct metadata));
	frame->data_len = 0;
}

void suo_frame_destroy(struct frame* frame) {
	if (frame->metadata != NULL) {
		free(frame->metadata);
		frame->metadata = NULL;
	}
	if (frame->data != NULL) {
		free(frame->data);
		frame->data = NULL;
		frame->data_len = 0;
		frame->data_alloc_len = 0;
	}
	free(frame);
}


void suo_frame_print(struct frame* frame, unsigned int flags) {
	if (frame == NULL)
		return;

#ifdef SUO_COLORS
	printf((flags & SUO_PRINT_COLOR) ? "\033[0;36m" : "\033[0;32m");
#endif

	if (flags & SUO_PRINT_COMPACT) {
		printf("Frame #%-6u %3d bytes, time: %-10lu\n",
			frame->hdr.id, frame->data_len, frame->hdr.timestamp);
#ifdef SUO_COLORS
		printf("\033[0m"); // Reset color modificator
#endif
		return;
	}

	printf("\n####\n");
	printf("# Frame #%-6u %3d bytes, time: %-10lu\n",
		frame->hdr.id, frame->data_len, frame->hdr.timestamp);

	if (flags & SUO_PRINT_METADATA) {
		printf("Metadata:\n  ");
		metadata_print(frame);
		printf("\n");
	}

#define NUM_COLUMNS   32

	if (flags & SUO_PRINT_DATA) {
		printf("Data:\n    ");
		for (unsigned int i = 0; i < frame->data_len; i++) {
			printf("%02x", frame->data[i]);
			printf(((i % NUM_COLUMNS) == NUM_COLUMNS - 1) ? "\n    " : " ");
		}
		printf("\n");
	}

#ifdef SUO_COLORS
	printf("\033[0m"); // Reset color modificator
#endif
}
