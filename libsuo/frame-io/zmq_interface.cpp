#include <string.h>
#include <iostream>


#include "zmq_interface.hpp"
#include "registry.hpp"

using namespace std;
using namespace suo;


// TODO: make these configurable
#define PRINT_DIAGNOSTICS
#define ENCODED_MAXLEN 0x900

/* One global ZeroMQ context, initialized only once */
zmq::context_t suo::zmq_ctx;


ZMQPublisher::Config::Config() {
	bind = "";
	connect = "";
	msg_format = ZMQMessageFormat::JSON;
}


ZMQPublisher::ZMQPublisher(const ZMQPublisher::Config& conf):
	conf(conf)
{
	if (conf.bind.empty() && conf.connect.empty())
		throw SuoError("Either bind or connect adddres was provided!");
	if (!conf.bind.empty() && !conf.connect.empty())
		throw SuoError("Both bind and connect adddres was provided!");

	// Connect the frame socket
	zmq_socket = zmq::socket_t(zmq_ctx, zmq::socket_type::pub);
	if (conf.bind.empty() == false) {
		cout << "Publisher binding: " << conf.bind << endl;
		zmq_socket.bind(conf.bind);
	}
	else {
		cout << "Publisher connecting: " << conf.connect << endl;
		zmq_socket.connect(conf.connect);
	}

}


ZMQPublisher::~ZMQPublisher()
{
	zmq_socket.close();
}


void ZMQPublisher::sinkFrame(const Frame& frame, Timestamp timestamp)
{
	switch (conf.msg_format) {
	case ZMQMessageFormat::StructuredBinary:
		suo_zmq_send_frame(zmq_socket, frame, zmq::send_flags::dontwait);
		break;
	case ZMQMessageFormat::RawBinary:
		suo_zmq_send_frame_raw(zmq_socket, frame, zmq::send_flags::dontwait);
		break;
	case ZMQMessageFormat::JSON:
		suo_zmq_send_frame_json(zmq_socket, frame, zmq::send_flags::dontwait);
		break;
	}
}


void ZMQPublisher::tick(Timestamp now)
{
#if 0
	zmq::message_t msg_hdr(sizeof(ZMQBinaryHeader));
	ZMQBinaryHeader* hdr = static_cast<ZMQBinaryHeader*>(msg_hdr.data());

	hdr->id = SUO_MSG_TIMING;
	hdr->flags = flags;
	hdr->timestamp = now;

	sinkFrame();
#endif
}


ZMQSubscriber::Config::Config() {
	bind = "";
	connect = "";
	msg_format = ZMQMessageFormat::JSON;
	subscribe = "";
}


ZMQSubscriber::ZMQSubscriber(const ZMQSubscriber::Config& conf):
	conf(conf)
{
	if (conf.bind.empty() && conf.connect.empty())
		throw SuoError("Either bind or connect adddres was provided!");
	if (!conf.bind.empty() && !conf.connect.empty())
		throw SuoError("Both bind and connect adddres was provided!");

	// Connect the frame socket
	zmq_socket = zmq::socket_t(zmq_ctx, zmq::socket_type::sub);
	if (conf.bind.empty() == false) {
		cout << "Subscriber binding: " << conf.bind << endl;
		zmq_socket.bind(conf.bind);
	}
	else {
		cout << "Subscriber connecting: " << conf.connect << endl;
		zmq_socket.connect(conf.connect);
	}

#if CPPZMQ_VERSION >= 40700
	zmq_socket.set(zmq::sockopt::subscribe, conf.subscribe);
#else
	zmq_socket.setsockopt(ZMQ_SUBSCRIBE, conf.subscribe);
#endif

}


ZMQSubscriber::~ZMQSubscriber()
{
	zmq_socket.close();
}


void ZMQSubscriber::reset()
{
	/* Flush possible queued frames */
	zmq::message_t msg;
#if CPPZMQ_VERSION >= 40700
	zmq_socket.set(zmq::sockopt::rcvtimeo, 500); // [ms]
#else
	zmq_socket.setsockopt(ZMQ_RCVTIMEO, 500); // [ms]
#endif
	while (1) {
		auto res = zmq_socket.recv(msg);
		if (res.has_value() == false)
			break;
	}
}



void ZMQSubscriber::sourceFrame(Frame& frame, Timestamp now)
{
	(void)now; // Not used since protocol stack doesn't run here

	int ret;

	switch (conf.msg_format) {
	case ZMQMessageFormat::StructuredBinary:
		ret = suo_zmq_recv_frame(zmq_socket, frame, zmq::recv_flags::dontwait);
		break;
	case ZMQMessageFormat::RawBinary:
		ret = suo_zmq_recv_frame_raw(zmq_socket, frame, zmq::recv_flags::dontwait);
		break;
	case ZMQMessageFormat::JSON:
		ret = suo_zmq_recv_frame_json(zmq_socket, frame, zmq::recv_flags::dontwait);
		break;
	}
#if 0
	if (ret == 1) {
		if (frame.id != SUO_MSG_SET) {
			//parse_set(frame);
		}

		if (frame.id != SUO_MSG_TRANSMIT) {
			cerr << "Warning: ZMQ input received non-transmit frame type: " << frame.id << endl;
		}

		//cout << frame(Frame::PrintData | Frame::PrintMetadata | Frame::PrintColored | Frame::PrintAltColor);
	}
#endif
}






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





void suo::suo_zmq_send_frame_raw(zmq::socket_t& sock, const Frame& frame, zmq::send_flags zmq_flags) {

	// Control frame without actual payload
	if (frame.data.empty())
		return;

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

	/* Dump JSON object to string and send it */
	try {
		string json_string = frame.serialize_to_json();
		sock.send(zmq::buffer(json_string), zmq::send_flags::dontwait);
	}
	catch (const zmq::error_t& e) {
		throw SuoError("ZMQ error in zmq_send_frame: %s", e.what());
	}

}



/* Receive a frame from ZMQ socket */
int suo::suo_zmq_recv_frame(zmq::socket_t& sock, Frame& frame, zmq::recv_flags flags) {
	zmq::message_t msg_hdr, msg_metadata, msg_data;

	/* Read the first part */
	try {
		auto res = sock.recv(msg_hdr, flags);
		if (!res) return 0;
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
		if (!res)
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
		if (!res)
			throw SuoError("zmq_recv_frame: Failed to read data.");
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


/* Receive a frame from ZMQ socket */
int suo::suo_zmq_recv_frame_raw(zmq::socket_t& sock, Frame& frame, zmq::recv_flags flags) {
	zmq::message_t msg_data;

	/* Read data */
	try {
		auto res = sock.recv(msg_data, zmq::recv_flags::dontwait);
		if (!res)
			return 0;
	}
	catch (const zmq::error_t& e) {
		throw SuoError("zmq_recv_frame: Failed to read ZMQ socket. %s", e.what());
	}

	if (msg_data.more() == true)
		throw SuoError("zmq_recv_frame: Confusion with more");

	frame.clear();

	// Copy frame data
	frame.data.resize(msg_data.size());
	memcpy(frame.data.data(), msg_data.data(), msg_data.size());

	return 1;
}


/* Receive a frame from ZMQ socket */
int suo::suo_zmq_recv_frame_json(zmq::socket_t& sock, Frame& frame, zmq::recv_flags flags) {
	zmq::message_t msg;

	/* Read the first part */
	try {
		auto res = sock.recv(msg, flags);
		if (!res)
			return 0;
	}
	catch (const zmq::error_t& e) {
		throw SuoError("zmq_recv_frame: Failed to read ZMQ socket. %s", e.what());
	}

	try {
		frame = Frame::deserialize_from_json(msg.to_string());
		return 1;
	}
	catch (const std::exception& e) {
		cerr << "Failed to parse received JSON message: " << e.what() << endl;
		frame.clear();
	}
	return 0;
}



Block* createZMQPublisher(const Kwargs& args) {
	return new ZMQPublisher();
}
static Registry registerZMQPublisher("ZMQPublisher", &createZMQPublisher);

Block* createZMQSubscriber(const Kwargs& args) {
	return new ZMQSubscriber();
}
static Registry registerZMQSubscriber("ZMQSubscriber", &createZMQSubscriber);
