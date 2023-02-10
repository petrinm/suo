#pragma once

#include "suo.hpp"
#include <zmq.hpp>


namespace suo
{


extern zmq::context_t zmq_ctx;

/* Suo message identifiers */
#define SUO_MSG_RECEIVE          0x0001
#define SUO_MSG_TRANSMIT         0x0002
#define SUO_MSG_TIMING           0x0003
#define SUO_MSG_SET              0x0004
#define SUO_MSG_GET              0x0005


#pragma pack(push, 1)
struct ZMQBinaryHeader
{
	uint32_t id;
	uint32_t flags;
	uint64_t timestamp;
};
struct ZMQBinaryMetadata
{
	char name[12];
	uint32_t type;
	uint64_t timestamp;
};
#pragma pack(pop)


enum ZMQMessageFormat {
	RawBinary,
	StructuredBinary,
	JSON,
};

/*
 */
class ZMQPublisher: public Block
{
public:

	/* ZMQ frame output configuration */
	struct Config {
		Config();

		/* Address which frame transmit socket will bind to.
		 * (Either bind or connected address should be provided!) */
		std::string bind;

		/* Address which frame transmit socket will connect to.
		 * (Either bind or connected address should be provided!) */
		std::string connect;

		/* Messaging format used over the socket */
		enum ZMQMessageFormat msg_format;

	};

	explicit ZMQPublisher(const Config& conf = Config());
	~ZMQPublisher();

	/* */
	void sinkFrame(const Frame& frame, Timestamp timestamp);

	/* Send a timing message */
	void tick(Timestamp now);

private:
	Config conf;
	zmq::socket_t zmq_socket;
};


class ZMQSubscriber: public Block
{
public:

	/* ZMQ frame output configuration */
	struct Config {
		Config();

		/* Address which frame transmit socket will bind to.
		 * (Either bind or connected address should be provided!) */
		std::string bind;

		/* Address which frame transmit socket will connect to.
		 * (Either bind or connected address should be provided!) */
		std::string connect;

		/* Messaging format used over the socket */
		enum ZMQMessageFormat msg_format;

		/* */
		std::string subscribe;
	};

	explicit ZMQSubscriber(const Config& conf = Config());
	~ZMQSubscriber();

	/* */
	void reset();

	/* */
	void sourceFrame(Frame& frame, Timestamp now);


private:
	Config conf;
	zmq::socket_t zmq_socket;
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
void suo_zmq_send_frame(zmq::socket_t& sock, const Frame& frame, zmq::send_flags flags);
void suo_zmq_send_frame_raw(zmq::socket_t& sock, const Frame& frame, zmq::send_flags flags);
void suo_zmq_send_frame_json(zmq::socket_t& sock, const Frame& frame, zmq::send_flags flags);

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
int suo_zmq_recv_frame(zmq::socket_t& sock, Frame& frame, zmq::recv_flags flags);
int suo_zmq_recv_frame_raw(zmq::socket_t& sock, Frame& frame, zmq::recv_flags flags);
int suo_zmq_recv_frame_json(zmq::socket_t& sock, Frame& frame, zmq::recv_flags flags);

}; // namespace suo
