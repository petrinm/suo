
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "suo.h"
#include "metadata.h"

#define SUO_COLORS


struct metadata* suo_metadata_set(struct frame* frame, unsigned ident, unsigned type) {
	if (frame->metadata == NULL)
		frame->metadata = calloc(1, MAX_METADATA * sizeof(struct metadata));

	if (frame->metadata_len < MAX_METADATA) {
		struct metadata* m = &frame->metadata[frame->metadata_len++];
		m->type = type;
		m->ident = ident;
		return m;
	}

	static struct metadata none;
	return &none;
}



void suo_metadata_name(const struct metadata* m, char* name) {
	switch (m->ident) {
	case METADATA_ID: strcpy(name, "id"); break;
	case METADATA_MODE: strcpy(name, "mode"); break;
	case METADATA_POWER: strcpy(name, "power"); break;
	case METADATA_RSSI: strcpy(name, "rssi"); break;
	case METADATA_CFO: strcpy(name, "cfo"); break;
	case METADATA_SYNC_ERRORS: strcpy(name, "sync_errors"); break;
	case METADATA_GOLAY_CODED: strcpy(name, "golay_coded"); break;
	case METADATA_GOLAY_ERRORS: strcpy(name, "golay_errors"); break;
	case METADATA_RS_BIT_ERRORS: strcpy(name, "rs_bit_error"); break;
	case METADATA_RS_OCTET_ERRORS: strcpy(name, "rs_octet_error"); break;
	default: sprintf(name, "unknown_%u", m->ident); break;
	}
}



void suo_metadata_print(const struct frame* frame /*, unsigned int flags*/ ) {
	assert(frame != NULL);

	char name[32];
	if (frame->metadata == NULL)
		return;

	for (int mi = 0; mi < frame->metadata_len; mi++) {
		const struct metadata* m = &frame->metadata[mi];

		if (m->ident == 0)
			break;
		if (m != frame->metadata) // Not the first item?
			printf(", ");

		suo_metadata_name(m, name);

		switch (m->type) {
	 	case METATYPE_FLOAT:  printf("%s = %f", name, m->v_float); break;
	 	case METATYPE_DOUBLE: printf("%s = %lf", name, m->v_double); break;
	 	case METATYPE_INT:    printf("%s = %d", name, m->v_int); break;
	 	case METATYPE_UINT:   printf("%s = %u", name, m->v_uint); break;
	 	case METATYPE_TIME:   printf("%s = %lu", name, m->v_time); break;
		}
	}
}
