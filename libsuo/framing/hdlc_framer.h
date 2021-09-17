#ifndef __SUO_FRAMING_HDLC_FRAMER_H__
#define __SUO_FRAMING_HDLC_FRAMER_H__

#include "suo.h"

struct hdlc_framer_conf {
	//
	unsigned int mode;

};

extern const struct encoder_code hdlc_framer_code;

#endif /* __SUO_FRAMING_HDLC_FRAMER_H__ */
