
#include "suo.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define SUO_COLORS


int suo_error(int err_code, const char* err_msg) {
	fprintf(stderr, "suo_error %s\n", err_msg);
	return err_code;
}


struct frame* suo_frame_new(unsigned int data_len) {
	struct frame* frame = (struct frame* )calloc(1, sizeof(struct frame));
	frame->metadata = (struct metadata*)calloc(MAX_METADATA, sizeof(struct metadata));
	frame->metadata_len = 0;
	if (data_len > 0)
		frame->data = (uint8_t*)calloc(1, data_len);
	frame->data_len = 0;
	frame->data_alloc = data_len;
	return frame;
}


struct frame* suo_frame_resize(struct frame* frame, unsigned int data_len) {
	if (frame == NULL)
		return suo_frame_new(data_len);
	if (data_len < frame->data_alloc)
		return frame;

	frame->data = (uint8_t*)realloc(frame->data, data_len);
	frame->data_len = 0;
	frame->data_alloc = data_len;

	return frame;
}


void suo_frame_clear(struct frame* frame) {
	if (frame == NULL)
		return;

	memset(&frame->hdr, 0, sizeof(struct frame_header));
	memset(frame->metadata, 0, MAX_METADATA * sizeof(struct metadata));

	frame->metadata_len = 0;
	frame->data_len = 0;
}


struct frame* suo_frame_copy(struct frame* dst, const struct frame* src) {
	assert(src == NULL);

	if (dst == NULL)
		dst = suo_frame_new(0);

	/* Copy header */
	memcpy(&dst->hdr, &src->hdr, sizeof(struct frame_header));

	/* Copy data */
	if (dst->data == NULL || dst->data_alloc < src->data_len) {
		dst->data = realloc(dst->data, src->data_len);
		dst->data_alloc = src->data_len;
	}
	memcpy(dst->data, src->data, src->data_len);
	dst->data_len = src->data_len;

	/* Copy metadata */
	if (dst->metadata == NULL /* || dst->metadata_len < src->metadata_len*/) {
		dst->metadata = realloc(dst->metadata, MAX_METADATA * sizeof(struct metadata));
		//dst->metadata_alloc = src->metadata_len;
	}
	memcpy(dst->metadata, src->metadata, sizeof(struct metadata) * MAX_METADATA); // src->metadata_len);

	return dst;
}

void suo_frame_destroy(struct frame* frame) {
	if (frame == NULL)
		return;

	if (frame->metadata != NULL) {
		free(frame->metadata);
		frame->metadata = NULL;
	}
	if (frame->data != NULL) {
		free(frame->data);
		frame->data = NULL;
		frame->data_len = 0;
		frame->data_alloc = 0;
	}
	free(frame);
}


void suo_frame_print(const struct frame* frame, unsigned int flags) {
	if (frame == NULL)
		return;

	FILE * stream = stdout;

#ifdef SUO_COLORS
	fprintf(stream, (flags & SUO_PRINT_COLOR) ? "\033[0;36m" : "\033[0;32m");
#endif

	if (flags & SUO_PRINT_COMPACT) {
		fprintf(stream, "Frame #%-6u %3d bytes, time: %-10lu\n",
			frame->hdr.id, frame->data_len, frame->hdr.timestamp);
		goto exit;
	}

	fprintf(stream, "\n####\n");
	fprintf(stream, "# Frame #%-6u %3d bytes, time: %-10lu\n",
		frame->hdr.id, frame->data_len, frame->hdr.timestamp);

	if (flags & SUO_PRINT_METADATA) {
		fprintf(stream, "Metadata:\n    ");
		if (frame->metadata[0].ident != 0)
			suo_metadata_print(frame);
		else
			fprintf(stream, "(none)");
		fprintf(stream, "\n");
	}

#define NUM_COLUMNS   32

	if (flags & SUO_PRINT_DATA) {
		fprintf(stream, "Data:\n    ");
		for (unsigned int i = 0; i < frame->data_len; i++) {
			fprintf(stream, "%02x", frame->data[i]);
			fprintf(stream, ((i % NUM_COLUMNS) == NUM_COLUMNS - 1) ? "\n    " : " ");
		}
		fprintf(stream, "\n");
	}

exit:
#ifdef SUO_COLORS
	fprintf(stream, "\033[0m"); // Reset color modificator
#endif
	fflush(stream);
}



void suo_print_symbols(symbol_t *symbols, size_t len) {
	while (len-- > 0)
		printf("%d ", *(symbols++));
	printf("\n\n");
}
