
#include "suo.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct metadata* set_meta(struct frame* frame, unsigned ident, unsigned type) {
	struct metadata* m = frame->m;
	for (int mi = 0; mi < MAX_METADATA; mi++) {
		if (m->type == 0) {
			m->type = type;
			m->ident = ident;
			return m;
		}
		m++;
	}
	static struct metadata none;
	return &none;
}



void metadata_name(struct metadata* m, char* name) {
	switch (m->ident) {
	case METADATA_ID: strcpy(name, "id"); break;
	case METADATA_MODE: strcpy(name, "mode"); break;
	case METADATA_POWER: strcpy(name, "power"); break;
	case METADATA_RSSI: strcpy(name, "rssi"); break;
	case METADATA_CFO: strcpy(name, "cfo"); break;
	case METADATA_SYNC_ERRORS: strcpy(name, "sync_errors"); break;
	case METADATA_GOLAY_CODED: strcpy(name, "golary_coded"); break;
	case METADATA_GOLAY_ERRORS: strcpy(name, "golay_errors"); break;
	case METADATA_RS_BIT_ERRORS: strcpy(name, "rs_bit_error"); break;
	case METADATA_RS_OCTET_ERRORS: strcpy(name, "rs_octet_error"); break;
	default: sprintf(name, "unknown_%d", m->ident); break;
	}
}

void metadata_print(struct frame* frame) {

	char name[32];
	struct metadata* m = frame->m;
	for (int mi = 0; mi < MAX_METADATA; mi++) {

		if (m->ident == 0)
			break;
		if (m != frame->m)
			printf(", ");

		metadata_name(m, name);

		switch (m->type) {
	 	case METATYPE_FLOAT: printf("%s = %f", name, m->fl); break;
	 	case METATYPE_DOUBLE: printf("%s = %lf", name, m->dl); break;
	 	case METATYPE_INT:  printf("%s = %d", name, m->i); break;
	 	case METATYPE_UINT:  printf("%s = %d", name, m->ui); break;
		}

		m++;
	}
}


int metadata_dump(uint8_t* out) {
	//
}

int metadata_load(uint8_t* out) {
	//
}
