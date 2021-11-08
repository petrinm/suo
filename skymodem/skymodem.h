#ifndef __SUOAPP_H__
#define __SUOAPP_H__


#include "suo.h"

#if 0
/* Everything combined */
struct suo {
	const struct receiver_code *receiver;
	void *receiver_arg;

	const struct transmitter_code *transmitter;
	void *transmitter_arg;

	const struct decoder_code *decoder;
	void *decoder_arg;

	const struct encoder_code *encoder;
	void *encoder_arg;

	const struct rx_output_code *rx_output;
	void *rx_output_arg;

	const struct tx_input_code *tx_input;
	void *tx_input_arg;

	const struct signal_io_code *signal_io;
	void *signal_io_arg;
};
#endif

#endif /* __SUOAPP_H__*/
