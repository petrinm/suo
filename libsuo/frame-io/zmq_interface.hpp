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



/*
 */
class ZMQOutput: public Block
{
public:

	/* ZMQ frame output configuration */
	struct Config {
		Config();

		/* Address for connecting or binding to transfer frame */
		std::string address;

		/* Address for connecting or binding to transfer ticks */
		std::string address_tick;

		/* Connection flags */
		bool binding;
		bool binding_ticks;
		bool thread;
	};

	ZMQOutput(const Config& conf = Config());
	~ZMQOutput();


	void sinkFrame(const Frame& frame, Timestamp timestamp);

	/* Send a timing */
	void tick(unsigned int flags, Timestamp now);

private:
	Config conf;

	/* Decoder thread */
	volatile bool decoder_running;
	pthread_t decoder_thread;

	/* ZeroMQ sockets */
	zmq::socket_t z_rx_pub; /* Publish decoded frames */
	zmq::socket_t z_tick_pub; /* Publish ticks */
	zmq::socket_t z_decw; 
	zmq::socket_t z_decr; /* Receiver-to-decoder queue */

	/* Callbacks */
	//const struct decoder_code* decoder;

};



/*
 */
class ZMQInput : public Block
{
public:

	/* ZMQ frame input configuration */
	struct Config {
		Config();

		/* Address for connecting or binding to transfer frame */
		std::string address;

		/* Address for connecting or binding to transfer ticks */
		std::string address_tick;

		/* Connection flags */
		bool binding;
		bool binding_ticks;
		bool thread;
	};

	ZMQInput(const Config& args = Config());
	~ZMQInput();

	void reset();

	void sourceFrame(Frame& frame, Timestamp now);

	void tick(unsigned int flags, Timestamp now);

private:

	/* Configuration */
	Config conf;

	/* Encoder thread */
	bool encoder_running;
	pthread_t encoder_thread;

	/* ZeroMQ sockets */
	zmq::socket_t z_tx_sub; /* Subscribe frames to be encoded */
	zmq::socket_t z_tick_pub; /* Publish ticks */
	zmq::socket_t z_txbuf_w;
	zmq::socket_t z_txbuf_r; /* Encoded-to-transmitter queue */

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
int suo_zmq_recv_frame_json(zmq::socket_t& sock, Frame& frame, zmq::recv_flags flags);

}; // namespace suo
