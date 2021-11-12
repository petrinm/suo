/*
 * Suo Metadata
 */
#ifndef __SUO_METADATA_H__
#define __SUO_METADATA_H__

#include <stdint.h>
#include "suo.h"

#define MAX_METADATA 16



/* Metadata identities */
#define METADATA_ID               1
#define METADATA_MODE             10
#define METADATA_POWER            20
#define METADATA_RSSI             21
#define METADATA_CFO              30
#define METADATA_FREQEST          31
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
#define SET_METADATA_FLOAT(frame, ident, val)  suo_metadata_set(frame, ident, METATYPE_FLOAT)->v_float = (val)
#define SET_METADATA_DOUBLE(frame, ident, val) suo_metadata_set(frame, ident, METATYPE_DOUBLE)->v_double = (val)
#define SET_METADATA_INT(frame, ident, val)    suo_metadata_set(frame, ident, METATYPE_INT)->v_int = (val)
#define SET_METADATA_UINT(frame, ident, val)   suo_metadata_set(frame, ident, METATYPE_UINT)->v_uint = (val)
#define SET_METADATA_TIME(frame, ident, val)   suo_metadata_set(frame, ident, METATYPE_TIME)->v_time = (val)


/* Metadata struct */
struct metadata {
	//
	struct {
		uint8_t len;
		uint8_t type;
		uint16_t ident;
	} __attribute__((packed));

	// Union for the actual data
	union {
		int32_t     v_int;
		uint32_t    v_uint;
		float       v_float;
		double      v_double;
		suo_timestamp_t v_time;
		uint8_t     raw[16];
	};
} __attribute__((packed));

struct frame;

/*
 */
struct metadata* suo_metadata_set(struct frame* frame, unsigned ident, unsigned type);

/*
 */
void suo_metadata_print(const struct frame* frame);

/*
 */
int suo_metadata_dump(uint8_t* out);

/*
 */
int suo_metadata_load(uint8_t* out);


#endif /* __SUO_METADATA_H__ */
