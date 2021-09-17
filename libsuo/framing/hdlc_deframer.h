#ifndef __SUO_FRAMING_HDLC_DEFRAMER_H__
#define __SUO_FRAMING_HDLC_DEFRAMER_H__

#include "suo.h"

struct hdlc_deframer_conf {
	/* NRZ, NRZ-I, G3RUH */
	unsigned int mode;

};

extern const struct decoder_code hdlc_deframer;

#endif /* __SUO_FRAMING_HDLC_DEFRAMER_H__ */
