#ifdef SUO_SUPPORT_AMQP

#include "frame-io/amqp_interface.hpp"

#include <iostream>
#include <memory>
#include <algorithm> // max

#include "json.hpp"
#include <sys/select.h>

using namespace std;
using namespace suo;
using json = nlohmann::json;

AMQPInterface::Config::Config() {
	amqp_url = "amqp://guest:guest@localhost/";
}

AMQPInterface::AMQPInterface(const Config& conf_):
	conf(conf_),
	connection(this, AMQP::Address(conf.amqp_url)),
	channel(&connection)
{

	channel.onError([](const char* message) {
		cout << "AMQP channel error: " << message << endl;
	});

	if (conf.rx_routing_key.empty() == false) {

		// Create a queue, bind it and start consuming
		channel.declareQueue(AMQP::exclusive)
		.onSuccess([this](const string &name, uint32_t messagecount, uint32_t consumercount) {
			
			cout << "Binding AMQP exchange: " << conf.exchange << " with routing key: " << conf.rx_routing_key << endl;
			
			channel.bindQueue(conf.exchange, name, conf.rx_routing_key);

			channel.consume(name)
			.onReceived([this](const AMQP::Message& message, uint64_t deliveryTag, bool redelivered) {
				message_callback(message, deliveryTag, redelivered);
			});

		});
	}

	//.onSuccess(startCb).onCancelled(cancelledCb).onError(errorCb);
}

AMQPInterface::~AMQPInterface() {
	cout << "destuctor!" << endl;
	channel.close();
	connection.close();	
}


void AMQPInterface::tick(suo::Timestamp now)
{
	(void)now;
	for (int fd : all_fds)
		connection.process(fd, AMQP::readable | AMQP::writable);
}

void AMQPInterface::sinkFrame(const Frame& frame, Timestamp timestamp) {

	json dict;
	dict["id"] = frame.id;
	
	if ((frame.flags & Frame::Flags::has_timestamp) != Frame::Flags::none)
		dict["timestamp"] = frame.timestamp;


	/* Format metadata to a JSON dictionary */
	json meta_dict;
	for (const Metadata& metadata : frame.metadata) {
		//meta_dict[metadata.ident] = metadata.value;
	}
	dict["metadata"] = meta_dict;

	/* Format binary data to hexadecimal string */
	stringstream hexa_stream;
	hexa_stream << setfill('0') << hex;
	for (size_t i = 0; i < frame.size(); i++)
		hexa_stream << setw(2) << (int)frame.data[i];
	dict["data"] = hexa_stream.str();

	/* Dump JSON object to string and send it */
	channel.publish(conf.exchange, conf.tx_routing_key, dict.dump());
	cout << dict.dump() << endl;
}

void AMQPInterface::sourceFrame(Frame& frame, Timestamp now) {
	//frame = read_fifo();

}


/*
 */
void AMQPInterface::message_callback(const AMQP::Message& message, uint64_t deliveryTag, bool redelivered)
{
	(void)deliveryTag;
	(void)redelivered;

	cerr << "message_callback" << message.exchange() << " " << message.routingkey() << endl;
	cerr << string_view(message.body(), message.bodySize()) << endl;

	auto frame = Frame::deserialize_from_json(string(message.body(), message.bodySize()));
	cout << frame;
}

void AMQPInterface::onAttached(AMQP::TcpConnection* connection)
{
	(void)connection;
	cout << "AMQP Attached!" << endl;
}

void AMQPInterface::onConnected(AMQP::TcpConnection* connection)
{
	(void)connection;
	cout << "AMQP Connected!" << endl;
}

void AMQPInterface::onReady(AMQP::TcpConnection* connection)
{
	(void)connection;
	cout << "AMQP Ready!" << endl;
}

void AMQPInterface::onError(AMQP::TcpConnection* connection, const char* message)
{
	(void)connection;
	cout << "AMQP Error!" << message << endl;
}

void AMQPInterface::onClosed(AMQP::TcpConnection* connection)
{
	(void)connection;
	cout << "AMQP Closed!" << endl;
}

void AMQPInterface::onLost(AMQP::TcpConnection* connection)
{
	(void)connection;
	cout << "AMQP Lost!" << endl;
}

void AMQPInterface::onDetached(AMQP::TcpConnection* connection)
{
	(void)connection;
	cout << "AMQP Detached!" << endl;
}

void AMQPInterface::monitor(AMQP::TcpConnection* connection, int fd, int flags)
{
	(void)connection;
	//cout << "AMQP Monitor " << fd << " " << flags << endl;

	if (flags == 0) {
		r_fds.erase(fd);
		w_fds.erase(fd);
		all_fds.erase(fd);
	}
	else {
		all_fds.insert(fd);
		if (flags & AMQP::readable)
			r_fds.insert(fd);
		if (flags & AMQP::writable)
			w_fds.insert(fd);
	}

}

#endif
