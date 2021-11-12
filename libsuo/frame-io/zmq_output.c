#include <string.h>
#include <assert.h>
#include <zmq.h>
#include <pthread.h>
#include <signal.h>

#include "zmq_interface.h"
#include "suo_macros.h"

// TODO: make these configurable
#define PRINT_DIAGNOSTICS
#define BITS_MAXLEN 0x900
#define DECODED_MAXLEN 0x200

/* One global ZeroMQ context, initialized only once */
void *zmq = NULL;


static void print_fail_zmq(const char *function, int ret)
{
	fprintf(stderr, "%s failed (%d): %s\n", function, ret, zmq_strerror(errno));
}
#define ZMQCHECK(function) do { int ret = (function); if(ret < 0) { print_fail_zmq(#function, ret); goto fail; } } while(0)


struct zmq_output {
	/* Configuration */
	struct zmq_rx_output_conf c;

	/* Decoder thread */
	volatile bool decoder_running;
	pthread_t decoder_thread;

	/* ZeroMQ sockets */
	void *z_rx_pub; /* Publish decoded frames */
	void *z_tick_pub; /* Publish ticks */
	void *z_decw, *z_decr; /* Receiver-to-decoder queue */

	/* Callbacks */
	const struct decoder_code *decoder;
	void *decoder_arg;
};

static int zmq_output_destroy(void *arg);
static void *zmq_decoder_main(void*);


static void *zmq_output_init(const void *confv)
{
	const struct zmq_rx_output_conf *conf = (const struct zmq_rx_output_conf *)confv;
	struct zmq_output *self = (struct zmq_output*)calloc(1, sizeof(*self));
	self->c = *(struct zmq_rx_output_conf*)confv;

	if(zmq == NULL)
		zmq = zmq_ctx_new();

	// Connect the frame socket
	self->z_rx_pub = zmq_socket(zmq, ZMQ_PUB);
	if (self->c.binding) {
		printf("Output binded to: %s\n", self->c.address);
		ZMQCHECK(zmq_bind(self->z_rx_pub, self->c.address));
	}
	else {
		printf("Output connected to: %s\n", self->c.address);
		ZMQCHECK(zmq_connect(self->z_rx_pub, self->c.address));
	}

	// Connect the tick socket
	if (self->c.address_tick != NULL && strlen(self->c.address_tick) > 0) {
		self->z_tick_pub = zmq_socket(zmq, ZMQ_PUB);
		if (self->c.binding_ticks) {
			printf("Output ticks binded to: %s\n", self->c.address_tick);
			ZMQCHECK(zmq_bind(self->z_tick_pub, self->c.address_tick));
		}
		else {
			printf("Output ticks connected to: %s\n", self->c.address_tick);
			ZMQCHECK(zmq_connect(self->z_tick_pub, self->c.address_tick));
		}
	}

	return self;

fail: // TODO cleanup
	zmq_output_destroy(self);
	return NULL;
}


static int zmq_output_set_callbacks(void *arg, const struct decoder_code *decoder, void *decoder_arg)
{
	struct zmq_output *self = (struct zmq_output*)arg;
	self->decoder = decoder;
	self->decoder_arg = decoder_arg;

	/* Create the decoder thread only if a decoder is set */
	if (decoder != NULL) {
		/* Decoder runs in a separate thread.
		* ZeroMQ inproc pair transfers the frames to be decoded.
		* Initialize writing and reading ends of the pair
		* and then start a thread. */

		/* Create unique name in case multiple instances are initialized */
		char pair_name[20];
		static char pair_number=0;
		snprintf(pair_name, 20, "inproc://dec_%d", ++pair_number);

		self->z_decr = zmq_socket(zmq, ZMQ_PAIR);
		ZMQCHECK(zmq_bind(self->z_decr, pair_name));
		self->z_decw = zmq_socket(zmq, ZMQ_PAIR);
		ZMQCHECK(zmq_connect(self->z_decw, pair_name));

		self->decoder_running = 1;
		pthread_create(&self->decoder_thread, NULL, zmq_decoder_main, self);
	}
	return 0;
fail:
	return -1;
}


static int zmq_output_destroy(void *arg)
{
	struct zmq_output *self = (struct zmq_output*)arg;
	if(self == NULL) return 0;
	if(self->decoder_running) {
		self->decoder_running = 0;
		pthread_kill(self->decoder_thread, SIGTERM);
		pthread_join(self->decoder_thread, NULL);
	}

	if (self->z_rx_pub != NULL) {
		zmq_close(self->z_rx_pub);
		self->z_rx_pub = NULL;
	}

	if (self->z_tick_pub != NULL) {
		zmq_close(self->z_tick_pub);
		self->z_tick_pub = NULL;
	}

	return 0;
}


static void *zmq_decoder_main(void *arg)
{
	struct zmq_output *self = (struct zmq_output*)arg;

	struct frame *coded = suo_frame_new(DECODED_MAXLEN);
	struct frame *decoded = suo_frame_new(DECODED_MAXLEN);

	/* Read frames from the receiver-to-decoder queue
	 * transmit buffer queue. */
	while(self->decoder_running) {
		int nread;
		zmq_msg_t input_msg;
		zmq_msg_init(&input_msg);
		nread = zmq_msg_recv(&input_msg, self->z_decr, 0);
		if(nread >= 0) {

			// TODO
			//if (self->decoder->decode(self->decoder_arg, zmq_msg_data(&input_msg), decoded, DECODED_MAXLEN) < 0)
			//	continue;

			printf("Decoded:\n");
			suo_frame_print(decoded, SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);

			if (suo_zmq_send_frame(self->z_rx_pub, decoded) < 0)
				goto fail;

		} else {
			print_fail_zmq("zmq_recv", nread);
			goto fail;
		}
		zmq_msg_close(&input_msg);
	}

fail:
	return NULL;
}



static int zmq_output_sink_frame(void *arg, const struct frame *frame, suo_timestamp_t timestamp)
{
	struct zmq_output *self = (struct zmq_output*)arg;

	suo_frame_print(frame, SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);

	/* Read straight from the subscriber socket
	 * if decoder thread and its socket is not created */
	void *s = self->z_decw;
	if (s == NULL)
		s = self->z_rx_pub; // return -1;

	return suo_zmq_send_frame(s, frame);
}


/* Send a timing */
static int zmq_output_tick(void *arg, unsigned int flags, suo_timestamp_t timenow)
{
	struct zmq_output *self = (struct zmq_output*)arg;
	void *s = self->z_tick_pub;
	if (s == NULL)
		return -1;

	struct frame_header msg = {
		.id = SUO_FLAGS_TIMING,
		.flags = flags,
		.timestamp = timenow,
	};
	ZMQCHECK(zmq_send(s, &msg, sizeof(msg), ZMQ_DONTWAIT));

	return SUO_OK;
fail: /* For ZMQCHECK macro */
	return -2;
}


const struct zmq_rx_output_conf zmq_rx_output_defaults = {
	.address = "tcp://*:43300",
	.binding = 1,
	.address_tick = "tcp://*:43302",
	.binding_ticks = 0,
	.thread = 1,
};

CONFIG_BEGIN(zmq_rx_output)
CONFIG_C(address)
CONFIG_C(address_tick)
CONFIG_I(binding)
CONFIG_I(binding_ticks)
CONFIG_I(thread)
CONFIG_END()

const struct rx_output_code zmq_rx_output_code = {
	.name = "zmq_output",
	.init = zmq_output_init,
	.destroy = zmq_output_destroy,
	.init_conf = init_conf,
	.set_conf = set_conf,
	.sink_frame = zmq_output_sink_frame,
	.sink_tick = zmq_output_tick
};
