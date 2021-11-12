#include <string.h>
#include <assert.h>
#include <zmq.h>
#include <pthread.h>
#include <signal.h>

#include "zmq_interface.h"
#include "suo_macros.h"

// TODO: make these configurable
#define PRINT_DIAGNOSTICS
#define ENCODED_MAXLEN 0x900

/* One global ZeroMQ context, initialized only once */
extern void *zmq;

static void print_fail_zmq(const char *function, int ret)
{
	fprintf(stderr, "%s failed (%d): %s\n", function, ret, zmq_strerror(errno));
}
#define ZMQCHECK(function) do { int ret = (function); if(ret < 0) { print_fail_zmq(#function, ret); goto fail; } } while(0)


struct zmq_input {
	/* Configuration */
	struct zmq_tx_input_conf c;

	/* Encoder thread */
	volatile bool encoder_running;
	pthread_t encoder_thread;

	/* ZeroMQ sockets */
	void *z_tx_sub; /* Subscribe frames to be encoded */
	void *z_tick_pub; /* Publish ticks */
	void *z_txbuf_w, *z_txbuf_r; /* Encoded-to-transmitter queue */

	/* Callbacks */
	const struct encoder_code *encoder;
	void *encoder_arg;
};


static int zmq_input_destroy(void *);
static void *zmq_encoder_main(void*);


static void *zmq_input_init(const void *confv)
{
	const struct zmq_tx_input_conf *conf = confv;
	struct zmq_input *self = (struct zmq_input *)calloc(1, sizeof(*self));
	self->c = *(struct zmq_tx_input_conf*)confv;

	self->z_txbuf_r = NULL;

	/* If this is called from another thread than zmq_output_init,
	 * a race condition is possible where two contexts are created.
	 * Just initialize everything in one thread to avoid problems. */
	if(zmq == NULL)
		zmq = zmq_ctx_new();

	// Connect the frame socket
	self->z_tx_sub = zmq_socket(zmq, ZMQ_SUB);
	if (self->c.binding) {
		printf("Input binded: %s\n", conf->address);
		ZMQCHECK(zmq_bind(self->z_tx_sub, conf->address));
	}
	else {
		printf("Input connected: %s\n", conf->address);
		ZMQCHECK(zmq_connect(self->z_tx_sub, conf->address));
	}
	ZMQCHECK(zmq_setsockopt(self->z_tx_sub, ZMQ_SUBSCRIBE, "", 0));

	// Connect the tick socket if wanted
	if (conf->address_tick != NULL && strlen(conf->address_tick) > 0) {
		self->z_tick_pub = zmq_socket(zmq, ZMQ_PUB);
		if (self->c.binding_ticks) {
			printf("Input ticks binded to: %s\n", conf->address_tick);
			ZMQCHECK(zmq_bind(self->z_tick_pub, conf->address_tick));
		}
		else {
			printf("Input ticks binded to: %s\n", conf->address_tick);
			ZMQCHECK(zmq_connect(self->z_tick_pub, conf->address_tick));
		}
	}

	return self;
fail:
	zmq_input_destroy(self);
	return NULL;
}


static int zmq_input_set_callbacks(void *arg, const struct encoder_code *encoder, void *encoder_arg)
{
	struct zmq_input *self = (struct zmq_input *)arg;
	self->encoder_arg = encoder_arg;
	self->encoder = encoder;

	/* Create the encoder thread only if an encoder is set */
	if (encoder != NULL) {
		/* Create a socket for inter-thread communication.
		 * Create unique name in case multiple instances are initialized */
		char pair_name[20];
		static char pair_number=0;
		snprintf(pair_name, 20, "inproc://txbuf_%d", ++pair_number);

		self->z_txbuf_r = zmq_socket(zmq, ZMQ_PAIR);
		ZMQCHECK(zmq_bind(self->z_txbuf_r, pair_name));
		self->z_txbuf_w = zmq_socket(zmq, ZMQ_PAIR);
		ZMQCHECK(zmq_connect(self->z_txbuf_w, pair_name));

		self->encoder_running = 1;

		pthread_create(&self->encoder_thread, NULL, zmq_encoder_main, self);
	}
	return 0;
fail:
	return -1;
}



/* Send a frame to ZMQ socket */
int suo_zmq_send_frame(void* sock, const struct frame *frame) {
	assert(sock != NULL && frame != NULL);
	int ret;

	/* Send frame header field */
	ret = zmq_send(sock, &frame->hdr, sizeof(struct frame_header), ZMQ_SNDMORE | ZMQ_DONTWAIT);
	if(ret < 0) {
		print_fail_zmq("zmq_send_frame:hdr", ret);
		goto fail;
	}

	// Control frame without actual payload
	if (frame->data == NULL)
		return 0;

	/* Send frame metadata */
	ret = zmq_send(sock, frame->metadata, frame->metadata_len * sizeof(struct metadata), ZMQ_SNDMORE | ZMQ_DONTWAIT);
	if(ret < 0) {
		print_fail_zmq("zmq_send_frame:meta", ret);
		goto fail;
	}

	/* Send frame data */
	ret = zmq_send(sock, frame->data, frame->data_len, ZMQ_DONTWAIT);
	if(ret < 0) {
		print_fail_zmq("zmq_send_frame:data", ret);
		goto fail;
	}

	return 0;
fail:
	return -1;
}


/* Receive a frame from ZMQ socket */
int suo_zmq_recv_frame(void* sock, struct frame *frame) {
	assert(sock != NULL);
	int ret;

	int64_t more = 0;
	size_t more_size = sizeof(more);

	frame->data_len = 0;
	frame->metadata_len = 0;

	/* Read the first part */
	ret = zmq_recv(sock, &frame->hdr, sizeof(struct frame_header), ZMQ_DONTWAIT);
	if (ret == 0 || (ret == -1 && errno == EAGAIN))
		return 0;  /* No frame in queue */

	if (ret < 0) {
		fprintf(stderr, "zmq_recv_frame: Failed to read socket %d %s\n", ret, zmq_strerror(errno));
		return ret;
	}

	if (ret != sizeof(struct frame_header)) {
		fprintf(stderr, "zmq_recv_frame: Header field size missmatch %d\n", ret);
		return -100;
	}
	zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &more_size);

	// If case of control frame, there are no more parts.
	if (more == 0)
		return 1;

	// Make sure we have an allocation for metadata
	if (frame->metadata == NULL) {
		frame->metadata = (struct metadata*)calloc(MAX_METADATA, sizeof(struct metadata));
		frame->metadata_len = 0;
	}

	/* Read metadata */
	ret = zmq_recv(sock, frame->metadata, MAX_METADATA * sizeof(struct metadata), ZMQ_DONTWAIT);
	if (ret < 0) {
		fprintf(stderr, "zmq_recv_frame: Failed to read metadata %d %s\n", ret, zmq_strerror(errno));
		return ret;
	}
	if (ret % sizeof(struct metadata) != 0) {
		fprintf(stderr, "zmq_recv_frame: Amount off metadata is strange %d\n", ret);
		return -102;
	}

	if (zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &more_size) < 0 || more != 1) {
		fprintf(stderr, "zmq_recv_frame: Confustion with more: %ld\n", more);
		return -103;
	}

	// Make sure we have an allocation
	//frame = suo_frame_resize(frame, 256);
	if (frame->data == NULL || frame->data_alloc == 0)
		return -104;

	/* Read data */
	ret = zmq_recv(sock, frame->data, frame->data_alloc, ZMQ_DONTWAIT);
	if (ret < 0) {
		fprintf(stderr, "zmq_recv_frame: failed to read data %d %s\n", ret, zmq_strerror(errno));
		return ret;
	}
	frame->data_len = ret;

	if (zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &more_size) < 0 || more != 0) {
		fprintf(stderr, "zmq_recv_frame: Confustion with more: %ld\n", more);
		return -105;
	}

	return 1;
}


static void *zmq_encoder_main(void *arg)
{
	struct zmq_input *self = (struct zmq_input *)arg;
	assert(self->encoder && self->encoder_arg && "No encoder");
	int ret;

	struct frame *uncoded = suo_frame_new(ENCODED_MAXLEN);
	struct frame *encoded = suo_frame_new(ENCODED_MAXLEN);

	/* Read frames from the SUB socket, encode them and put them
	 * in the transmit buffer queue. */
	while (self->encoder_running) {

		// Receive from interprocess PAIR
		if ((ret = suo_zmq_recv_frame(self->z_tx_sub, encoded)) < 0) {
			fprintf(stderr, "zmq_recv_frame: %d\n", ret);
			//break;
			continue;
		}

		//suo_frame_print(uncoded, SUO_PRINT_DATA | SUO_PRINT_METADATA);

		// Pass the bytes to encoder
		if (self->encoder->encode(self->encoder_arg, uncoded, encoded, ENCODED_MAXLEN) < 0)
			continue; // Encode failed, should not happen


		//assert(nbits <= ENCODED_MAXLEN);
		suo_zmq_send_frame(self->z_txbuf_w, encoded);

	}

	self->encoder_running = false;
	return NULL;
}



static int zmq_input_source_frame(void* arg, struct frame *frame, suo_timestamp_t timenow)
{
	struct zmq_input *self = (struct zmq_input *)arg;
	(void)timenow; // Not used since protocol stack doesn't run here

	/* If encoder is not set, the encoder thread is not created
	 * and the inter-thread socket isn't created either.
	 * Read straight from the subscriber socket in that case. */
	void *s = self->z_txbuf_r;
	if (s == NULL)
		s = self->z_tx_sub;

	int ret = suo_zmq_recv_frame(s, frame);
	if (ret == 1)
		suo_frame_print(frame, SUO_PRINT_DATA);
	return ret;
}


static int zmq_input_destroy(void *arg)
{
	struct zmq_input *self = (struct zmq_input *)arg;
	if(self == NULL) return 0;
	if(self->encoder_running) {
		self->encoder_running = 0;
		pthread_kill(self->encoder_thread, SIGTERM);
		pthread_join(self->encoder_thread, NULL);
	}

	if (self->z_tx_sub != NULL) {
		zmq_close(self->z_tx_sub);
		self->z_tx_sub = NULL;
	}

	if (self->z_tick_pub != NULL) {
		zmq_close(self->z_tick_pub);
		self->z_tick_pub = NULL;
	}

	return 0;
}


static int zmq_input_tick(void *arg, unsigned int flags, suo_timestamp_t timenow)
{
	struct zmq_input *self = (struct zmq_input *)arg;
	void *s = self->z_tick_pub;
	if (s == NULL)
		goto fail;

	struct frame_header msg = {
		.id = SUO_FLAGS_TIMING,
		.flags = flags,
		.timestamp = timenow,
	};

	ZMQCHECK(zmq_send(s, &msg, sizeof(msg), ZMQ_DONTWAIT));
	return 0;
fail: /* For ZMQCHECK macro */
	return -1;
}

static int set_frame_sink(void *arg, const struct encoder_code * sink, void *sink_arg) {
	struct zmq_input *self = (struct zmq_input *)arg;
	return 0;
}


const struct zmq_tx_input_conf zmq_tx_input_defaults = {
	.address = "tcp://*:43301",
	.binding = 1,
#if 1
	// transmit ticks in a separate socket
	.address_tick = "tcp://*:43303",
	.binding_ticks = 1,
#else
	// transmit ticks in the RX socket
	.address_tick = "tcp://localhost:43300",
	.binding_ticks = 0,
#endif
	.thread = 1,
};

CONFIG_BEGIN(zmq_tx_input)
CONFIG_C(address)
CONFIG_C(address_tick)
CONFIG_I(binding)
CONFIG_I(binding_ticks)
CONFIG_I(thread)
CONFIG_END()


const struct tx_input_code zmq_tx_input_code = {
	.name = "zmq_input",
	.init = zmq_input_init,
	.destroy= zmq_input_destroy,
	.init_conf = init_conf,
	.set_conf = set_conf,
	.set_frame_sink = set_frame_sink,
	//.set_callbacks = zmq_input_set_callbacks,
	.source_frame = zmq_input_source_frame,
	.sink_tick = zmq_input_tick
};
