#include <string.h>
#include <iostream>

#include "json.hpp"

//#include <pthread.h>
//#include <signal.h>


#include "zmq_interface.hpp"
#include "registry.hpp"

// https://brettviren.github.io/cppzmq-tour/index.html

using namespace std;
using namespace suo;
using json = nlohmann::json;


// TODO: make these configurable
#define PRINT_DIAGNOSTICS
#define ENCODED_MAXLEN 0x900

/* One global ZeroMQ context, initialized only once */
zmq::context_t suo::zmq_ctx;


ZMQInput::Config::Config() {
	address = "tcp://*:43301";
	binding = 1;
#if 1
	// transmit ticks in a separate socket
	address_tick = "tcp://*:43303";
	binding_ticks = 1;
#else
	// transmit ticks in the RX socket
	address_tick = "tcp://localhost:43300";
	binding_ticks = 0;
#endif
	thread = 1;
}


ZMQInput::ZMQInput(const Config& conf) :
	conf(conf)
{
	// Connect the frame socket
	z_tx_sub = zmq::socket_t(zmq_ctx, zmq::socket_type::sub);
	if (conf.binding) {
		cout << "Input binding: " << conf.address << endl;
		z_tx_sub.bind(conf.address);
	}
	else {
		cout << "Input connecting: " << conf.address << endl;
		z_tx_sub.connect(conf.address);
	}
	z_tx_sub.set(zmq::sockopt::subscribe, "");

	// Connect the tick socket if wanted
	if (conf.address_tick.empty() == false) {
		z_tick_pub = zmq::socket_t(zmq_ctx, zmq::socket_type::pub);
		if (conf.binding_ticks) {
			cout << "Input ticks binded to: " << conf.address_tick << endl;
			z_tick_pub.bind(conf.address_tick);
		}
		else {
			cout << "Input ticks binded to: " << conf.address_tick << endl;
			z_tick_pub.connect(conf.address_tick);
		}
	}

}


ZMQInput::~ZMQInput()
{
	/*if(encoder_running) {
		encoder_running = 0;
		pthread_kill(encoder_thread, SIGTERM);
		pthread_join(encoder_thread, NULL);
	}*/

	z_tx_sub.close();
	z_tick_pub.close();
}



void ZMQInput::reset() {
	/* Flush possible queued frames */
	zmq::message_t msg;
	z_tx_sub.set(zmq::sockopt::rcvtimeo, 500); // [ms]
	while (1) {
		auto res = z_tx_sub.recv(msg);
		if (res.has_value() == false)
			break;
	}
}


#if 0
static int zmq_input_set_callbacks(void *arg, const struct encoder_code *encoder, void *encoder_arg)
{
	struct zmq_input *self = (struct zmq_input *)arg;
	encoder_arg = encoder_arg;
	encoder = encoder;

	/* Create the encoder thread only if an encoder is set */
	if (encoder != NULL) {
		/* Create a socket for inter-thread communication.
		 * Create unique name in case multiple instances are initialized */
		char pair_name[20];
		static char pair_number=0;
		snprintf(pair_name, 20, "inproc://txbuf_%d", ++pair_number);

		z_txbuf_r = zmq_socket(zmq, ZMQ_PAIR);
		ZMQCHECK(zmq_bind(z_txbuf_r, pair_name));
		z_txbuf_w = zmq_socket(zmq, ZMQ_PAIR);
		ZMQCHECK(zmq_connect(z_txbuf_w, pair_name));

		encoder_running = 1;

		pthread_create(&encoder_thread, NULL, zmq_encoder_main, self);
	}
	return 0;
fail:
	return -1;
}
#endif


void suo::suo_zmq_send_frame(zmq::socket_t& sock, const Frame& frame, zmq::send_flags zmq_flags) {

	zmq::message_t msg_hdr(sizeof(ZMQBinaryHeader));
	ZMQBinaryHeader* hdr = static_cast<ZMQBinaryHeader*>(msg_hdr.data());
	hdr->id = frame.id;
	//hdr->flags = frame.flags;
	hdr->timestamp = frame.timestamp;

	/* Send frame header field */
	try {
		sock.send(msg_hdr, zmq::send_flags::sndmore | zmq::send_flags::dontwait);
	}
	catch (const zmq::error_t& e) {
		throw SuoError("zmq_send_frame:hdr %s", e.what());
	}

	// Control frame without actual payload
	if (frame.data.empty())
		return;

	zmq::message_t msg_meta(frame.metadata.size() * sizeof(ZMQBinaryMetadata));
	std::vector<ZMQBinaryMetadata> metadata_buffer(frame.metadata.size());
	// TODO: fill the metadata

	/* Send frame metadata */
	try {
		sock.send(msg_meta, zmq::send_flags::sndmore | zmq::send_flags::dontwait);
	}
	catch (const zmq::error_t& e) {
		throw SuoError("zmq_send_frame:meta %s", e.what());
	}

	zmq::message_t msg_data(frame.data.size());
	memcpy(msg_data.data(), &frame.data[0], frame.data.size());

	/* Send frame data */
	try {
		sock.send(msg_data, zmq::send_flags::dontwait);
	}
	catch (const zmq::error_t& e) {
		throw SuoError("zmq_send_frame:data %s", e.what());
	}
}


void suo::suo_zmq_send_frame_json(zmq::socket_t& sock, const Frame& frame, zmq::send_flags zmq_flags) {

	json dict;
	dict["id"] = frame.id;
	//dict["flags"] = frame.flags;
	dict["id"] = frame.id;

	/* Format metadata to a JSON dictionary */
	json meta_dict;
	for (const Metadata& metadata: frame.metadata) {
		//meta_dict[metadata.ident] = metadata.value;
	}
	dict["metadata"] = meta_dict;


	/* Format binary data to hexadecimal string */
	std::stringstream hexa_stream;
	hexa_stream << std::setfill('0') << std::setw(2) << std::hex;
	for (size_t i = 0; i < frame.size(); i++)
		hexa_stream << frame.data[i];
	dict["data"] = hexa_stream.str();

	/* Dump JSON object to string and send it */
	try {
		sock.send(zmq::buffer(dict.dump()), zmq::send_flags::dontwait);
	}
	catch (const zmq::error_t& e) {
		throw SuoError("ZMQ error in zmq_send_frame: %s", e.what());
	}

}


/* Receive a frame from ZMQ socket */
int suo::suo_zmq_recv_frame_json(zmq::socket_t& sock, Frame& frame, zmq::recv_flags flags) {
	zmq::message_t msg;

	/* Read the first part */
	try {
		auto res = sock.recv(msg, flags);
		if (res.value() == 0)
			return 0;
	}
	catch (const zmq::error_t& e) {
		throw SuoError("zmq_recv_frame: Failed to read socket. %s", e.what());
	}

	/* Parse message */
	json dict;
	try {
		dict = json::parse(msg.to_string());
	}
	catch (const json::exception& e) {
		throw SuoError("Failed to parse JSON dictionary. %s", e.what());
	}

	frame.clear();
	frame.id = dict["id"];
	//frame.flags = dict["flags"];
	frame.timestamp = dict["timestamp"];



	/* Parse ASCII hexadecimal string to */
	string hex_string;
	if (hex_string.size() % 2 != 0)
		throw SuoError("asddsa");

	size_t frame_len = hex_string.size() / 2;
	frame.data.resize(frame_len);
	for (size_t i = 0; i < frame_len; i++)
		frame.data[i] = stoul(hex_string.substr(2*i, 2));


	return 1;
}



/* Receive a frame from ZMQ socket */
int suo::suo_zmq_recv_frame(zmq::socket_t& sock, Frame& frame, zmq::recv_flags flags) {
	zmq::message_t msg_hdr, msg_metadata, msg_data;

	/* Read the first part */
	try {
		auto res = sock.recv(msg_hdr, flags);
		if (res.value() == 0)
			return 0;
	}
	catch (const zmq::error_t& e) {
		throw SuoError("zmq_recv_frame: Failed to read message header. %s", e.what());
	}

	if (msg_hdr.empty())
		return 0;
	if (msg_hdr.size() != sizeof(ZMQBinaryHeader))
		throw SuoError("zmq_recv_frame: Header field size missmatch");


	// If case of control frame, there are no more parts.
	if (msg_hdr.more() == false)
		return 1;

	/* Read metadata */
	try {
		auto res = sock.recv(msg_metadata, zmq::recv_flags::dontwait);
		if (res.has_value() == false)
			throw SuoError("zmq_recv_frame: Failed to read metadata.");
	}
	catch (const zmq::error_t& e) {
		throw SuoError("zmq_recv_frame: Failed to read metadata. %s", e.what());
	}

	if (msg_metadata.size() % sizeof(ZMQBinaryMetadata) != 0)
		throw SuoError("zmq_recv_frame: Amount off metadata is strange!");
	if (msg_metadata.more() == false)
		throw SuoError("zmq_recv_frame: Confusion with more");


	/* Read data */
	try {
		auto res = sock.recv(msg_data, zmq::recv_flags::dontwait);
		if (res.has_value() == false)
			throw SuoError("zmq_recv_frame: Failed to read metadata.");
	}
	catch (const zmq::error_t& e) {
		throw SuoError("zmq_recv_frame: Failed to read data. %s", e.what());
	}

	if (msg_data.more() == true)
		throw SuoError("zmq_recv_frame: Confusion with more");


	// Parse header
	const ZMQBinaryHeader* hdr = static_cast<const ZMQBinaryHeader*>(msg_hdr.data());

	frame.clear();
	frame.id = hdr->id;
	//frame.flags = hdr->flags;
	frame.timestamp = hdr->timestamp;


	// parse metadata
	frame.metadata.clear();
	const ZMQBinaryMetadata* meta = static_cast<const ZMQBinaryMetadata*>(msg_metadata.data());
	const size_t meta_count = msg_metadata.size() / sizeof(ZMQBinaryMetadata);
	for (size_t i = 0; i < meta_count; i++) {

	}

	// Copy frame data
	frame.data.resize(msg_data.size());
	memcpy(frame.data.data(), msg_data.data(), msg_data.size());


	return 1;
}




#if 0
static void *zmq_encoder_main(void *arg)
{
	struct zmq_input *self = (struct zmq_input *)arg;
	assert(encoder && encoder_arg && "No encoder");
	int ret;

	struct frame *uncoded = suo_frame_new(ENCODED_MAXLEN);
	struct frame *encoded = suo_frame_new(ENCODED_MAXLEN);

	/* Read frames from the SUB socket, encode them and put them
	 * in the transmit buffer queue. */
	while (encoder_running) {

		// Receive from interprocess PAIR
		if ((ret = suo_zmq_recv_frame(z_tx_sub, encoded, 0)) < 0) {
			fprintf(stderr, "zmq_recv_frame: %d\n", ret);
			//break;
			continue;
		}

		cerr << *uncoded; // SUO_PRINT_DATA | SUO_PRINT_METADATA);

		// Pass the bytes to encoder
		if (encoder->encode(encoder_arg, uncoded, encoded, ENCODED_MAXLEN) < 0)
			continue; // Encode failed, should not happen


		//assert(nbits <= ENCODED_MAXLEN);
		suo_zmq_send_frame(z_txbuf_w, encoded, ZMQ_DONTWAIT);

	}

	encoder_running = false;
	return NULL;
}
#endif



void ZMQInput::sourceFrame(Frame& frame, Timestamp now)
{
	(void)now; // Not used since protocol stack doesn't run here

	int ret = suo_zmq_recv_frame(z_tx_sub, frame, zmq::recv_flags::dontwait);
	if (ret == 1) {
		if (frame.id != SUO_MSG_SET) {
			//parse_set(frame);
		}

		if (frame.id != SUO_MSG_TRANSMIT) {
			cerr << "Warning: ZMQ input received non-transmit frame type: " << frame.id << endl;
		}

		cout << frame; // SUO_PRINT_DATA;
	}

}


void ZMQInput::tick(unsigned int flags, Timestamp now)
{
	zmq::message_t msg_hdr(sizeof(ZMQBinaryHeader));
	ZMQBinaryHeader* hdr = static_cast<ZMQBinaryHeader*>(msg_hdr.data());

	hdr->id = SUO_MSG_TIMING;
	hdr->flags = flags;
	hdr->timestamp = now;

	z_tick_pub.send(msg_hdr, zmq::send_flags::dontwait);
}


Block* createZMQInput(const Kwargs& args)
{
	return new ZMQInput();
}

static Registry registerZMQInput("ZMQInput", &createZMQInput);
