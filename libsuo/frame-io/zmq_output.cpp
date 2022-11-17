#include <zmq.hpp>
//#include <pthread.h>

#include "zmq_interface.hpp"
#include "registry.hpp"

// TODO: make these configurable
#define PRINT_DIAGNOSTICS
#define BITS_MAXLEN 0x900
#define DECODED_MAXLEN 0x200

using namespace std;
using namespace suo;


ZMQOutput::Config::Config() {
	address = "tcp://*:43300";
	binding = 1;
	address_tick = "tcp://*:43302";
	binding_ticks = 0;
	thread = 1;
}

ZMQOutput::ZMQOutput(const Config& conf) :
	conf(conf)
{

	// Connect the frame socket
	z_rx_pub = zmq::socket_t(zmq_ctx, zmq::socket_type::pub);
	if (conf.binding) {
		cout << "Output binded to: " << conf.address << endl;
		z_rx_pub.bind(conf.address);
	}
	else {
		cout << "Output connected to: " << conf.address << endl;
		z_rx_pub.connect(conf.address);
	}

	// Connect the tick socket
	if (conf.address_tick.empty()) {
		z_tick_pub = zmq::socket_t(zmq_ctx, zmq::socket_type::pub);
		if (conf.binding_ticks) {
			cout << "Output ticks binded to: " << conf.address_tick << endl;
			z_tick_pub.bind(conf.address_tick);
		}
		else {
			cout << "Output ticks connected to: " << conf.address_tick << endl;
			z_tick_pub.connect(conf.address_tick);
		}
	}
}

ZMQOutput::~ZMQOutput() {
	/*if(decoder_running) {
		decoder_running = 0;
		pthread_kill(decoder_thread, SIGTERM);
		pthread_join(decoder_thread, NULL);
	}*/

	z_rx_pub.close();
	z_tick_pub.close();
}

#if 0
static int zmq_output_set_callbacks(void *arg, const struct decoder_code *decoder, void *decoder_arg)
{
	struct zmq_output *self = (struct zmq_output*)arg;
	decoder = decoder;
	decoder_arg = decoder_arg;

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

		z_decr = zmq_socket(zmq, ZMQ_PAIR);
		ZMQCHECK(zmq_bind(z_decr, pair_name));
		z_decw = zmq_socket(zmq, ZMQ_PAIR);
		ZMQCHECK(zmq_connect(z_decw, pair_name));

		decoder_running = 1;
		pthread_create(&decoder_thread, NULL, zmq_decoder_main, self);
	}
	return 0;
fail:
	return -1;
}



static void *zmq_decoder_main(void *arg)
{
	struct zmq_output *self = (struct zmq_output*)arg;

	struct frame *coded = suo_frame_new(DECODED_MAXLEN);
	struct frame *decoded = suo_frame_new(DECODED_MAXLEN);

	/* Read frames from the receiver-to-decoder queue
	 * transmit buffer queue. */
	while(decoder_running) {
		int nread;
		zmq_msg_t input_msg;
		zmq_msg_init(&input_msg);
		nread = zmq_msg_recv(&input_msg, z_decr, 0);
		if(nread >= 0) {

			// TODO
			//if (decoder->decode(decoder_arg, zmq_msg_data(&input_msg), decoded, DECODED_MAXLEN) < 0)
			//	continue;

			printf("Decoded:\n");
			suo_frame_print(decoded, SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);

			if (suo_zmq_send_frame(z_rx_pub, decoded, ZMQ_DONTWAIT) < 0)
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
#endif



void ZMQOutput::sinkFrame(const Frame &frame, Timestamp timestamp)
{
	cout << frame; // SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);

#if 0
	frame->hdr.id = SUO_MSG_RECEIVE;
	frame->hdr.flags = 0;

	if (frame->hdr.timestamp == 0)
		frame->hdr.timestamp = timestamp;
#endif
	suo_zmq_send_frame(z_rx_pub, frame, zmq::send_flags::dontwait);
}


/* Send a timing */
void ZMQOutput::tick(unsigned int flags, Timestamp now)
{
	zmq::message_t msg_hdr(sizeof(ZMQBinaryHeader));
	ZMQBinaryHeader* hdr = static_cast<ZMQBinaryHeader*>(msg_hdr.data());

	hdr->id = SUO_MSG_TIMING;
	hdr->flags = flags;
	hdr->timestamp = now;
	
	z_tick_pub.send(msg_hdr, zmq::send_flags::dontwait);
}


Block* createZMQOutput(const Kwargs& args)
{
	return new ZMQOutput();
}

static Registry registerZMQOutput("ZMQOutput", &createZMQOutput);
