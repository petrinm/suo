#ifndef __SUO_FRAMEIO_ZMQ_INTERFACE_H__
#define __SUO_FRAMEIO_ZMQ_INTERFACE_H__

#include "suo.h"

/* ZMQ frame output configuration */
struct zmq_rx_output_conf {

	/* Address for connecting or binding to transfer frame */
	const char *address;

	/* Address for connecting or binding to transfer ticks */
	const char *address_tick;

	/* Connection flags */
	unsigned int binding : 1;
	unsigned int binding_ticks : 1;
	unsigned int thread : 1;
};

/* ZMQ frame input configuration */
struct zmq_tx_input_conf {

	/* Address for connecting or binding to transfer frame */
	const char *address;

	/* Address for connecting or binding to transfer ticks */
	const char *address_tick;

	/* Connection flags */
	unsigned int binding : 1;
	unsigned int binding_ticks : 1;
	unsigned int thread : 1;
};


/*
 * Send a Suo frame to ZMQ socket.
 * Args:
 *   sock: ZMQ socket object
 *   frame: Suo frame object to be send
 * Returns:
 *   0 on success
 *   <0 on error
 */
int suo_zmq_send_frame(void* sock, const struct frame *frame);

/*
 * Read a Suo frame from ZMQ socket. The function will never block!
 * Args:
 *   sock: ZMQ socket object
 *   frame: Suo frame object where received frame will
 * Returns:
 *   0 on success but no new frames where available
 *   1 on success and new frame was received on stored to frame objects
 *   <0 on error
 */
int suo_zmq_recv_frame(void* sock, struct frame *frame);


/* Suo module definitions */
extern const struct rx_output_code zmq_rx_output_code;
extern const struct tx_input_code zmq_tx_input_code;

#endif /* __SUO_FRAMEIO_ZMQ_INTERFACE_H__ */
