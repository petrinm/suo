
#include "modem/demod_fsk_quad.h"
#include "modem/demod_fsk_mfilt.h"
#include "modem/demod_fsk_corrbank.h"

#include "modem/mod_fsk.h"
#include "modem/mod_gmsk.h"
#include "modem/mod_psk.h"

#include "framing/syncword_framer.h"
#include "framing/syncword_deframer.h"

#include "framing/hdlc_framer.h"
#include "framing/hdlc_deframer.h"

#include "framing/golay_framer.h"
#include "framing/golay_deframer.h"


/* Tables to list all the modules implemented */

const struct receiver_code *suo_receivers[] = {
	//&demod_fsk_quad_code,
	&demod_fsk_mfilt_code,
	//&demod_fsk_corrbank_code,
	//&burst_dpsk_receiver_code,
	NULL
};

const struct transmitter_code *suo_transmitters[] = {
	&mod_fsk_code,
	&mod_gmsk_code,
	&mod_psk_code,
	NULL
};

const struct decoder_code *suo_decoders[] = {
	//&syncword_deframer_code,
	//&hdlc_deframer_code,
	&golay_deframer_code,
	NULL
};

const struct encoder_code *suo_encoders[] = {
	//&syncword_framer_code,
	//&hdlc_framer_code,
	&golay_framer_code,
	NULL,
};
