/*
 * For
 */
#ifndef __SUO_METADATA_H__
#define __SUO_METADATA_H__

#include <stdint.h>


#define MAX_METADATA 16




#define METADATA_FREQEST    1

/* Flags to indicate that a metadata field is used */
#define METADATA_ID               1
#define METADATA_MODE             10
#define METADATA_POWER            20
#define METADATA_RSSI             21
#define METADATA_CFO              30
#define METADATA_SYNC_ERRORS      40
#define METADATA_GOLAY_CODED      41
#define METADATA_GOLAY_ERRORS     42
#define METADATA_RS_BIT_ERRORS    43
#define METADATA_RS_OCTET_ERRORS  44
#define METADATA_END_TIME         60

/* Metadata types */
#define METATYPE_RAW               0
#define METATYPE_FLOAT             1
#define METATYPE_DOUBLE            2
#define METATYPE_INT               3
#define METATYPE_UINT              4
#define METATYPE_TIME              5

/* Metadata macros */
#define SET_METADATA_F(frame, ident, val)   set_meta(frame, ident, METATYPE_FLOAT)->fl = val
#define SET_METADATA_D(frame, ident, val)   set_meta(frame, ident, METATYPE_DOUBLE)->dl = val
#define SET_METADATA_I(frame, ident, val)   set_meta(frame, ident, METATYPE_INT)->i = val
#define SET_METADATA_UI(frame, ident, val)  set_meta(frame, ident, METATYPE_UINT)->i = val

/* Metadata struct */
struct metadata {
	struct {
		unsigned len: 4;
		unsigned type: 4;
		unsigned ident: 16;
	};

	union {
		int i;
		unsigned int ui;
		float fl;
		double dl;
		//timestamp_t time;
	};
};

struct frame;

struct metadata* set_meta(struct frame* frame, unsigned ident, unsigned type);

void metadata_print(struct frame* frame);
int metadata_dump(uint8_t* out);
int metadata_load(uint8_t* out);

#endif /* __SUO_METADATA_H__ */
