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

		// TODO: Reconnect automatically after disconnection
	}

	auto now = std::chrono::steady_clock::now();
	next_heartbeat = now + std::chrono::duration<long int>(heartbeat_interval);

	//.onSuccess(startCb).onCancelled(cancelledCb).onError(errorCb);
}

AMQPInterface::~AMQPInterface() {
	channel.close();
	connection.close();	
}


void AMQPInterface::tick(Timestamp _now)
{
	(void)_now;
	for (int fd : fds)
		connection.process(fd, AMQP::readable | AMQP::writable);

	// Check hearthbeat timeout
	auto now = std::chrono::steady_clock::now();
	const auto timeout = std::chrono::duration<double, std::micro>(next_heartbeat - now);
	if (timeout.count() <= 0) {
		//cout << "AMQP heartbeat " << endl;
		connection.heartbeat();
		next_heartbeat = now + std::chrono::duration<long int>(heartbeat_interval);
	}
}

void AMQPInterface::sinkFrame(const Frame& frame, Timestamp timestamp) {
	string json_string = frame.serialize_to_json();
	channel.publish(conf.exchange, conf.tx_routing_key, json_string);
}


void AMQPInterface::sourceFrame(Frame& frame, Timestamp now) {
	if (frame_queue.empty() == false) {
		frame = frame_queue.front();
		frame_queue.pop();
		cout << frame(Frame::PrintData | Frame::PrintMetadata);
	}
}


/*
 */
void AMQPInterface::message_callback(const AMQP::Message& message, uint64_t deliveryTag, bool redelivered)
{
	(void)deliveryTag;
	(void)redelivered;

	//cerr << "message_callback" << message.exchange() << " " << message.routingkey() << endl;
	//cerr << string_view(message.body(), message.bodySize()) << endl;

	try {
		frame_queue.push(Frame::deserialize_from_json(string(message.body(), message.bodySize())));
	}
	catch (const std::exception& e) {
		cerr << string_view(message.body(), message.bodySize()) << endl;
		cerr << "Failed to parse received JSON message: " << e.what() << endl;
	}
}

void AMQPInterface::onAttached(AMQP::TcpConnection* connection)
{
	(void)connection;
	//cout << "AMQP Attached!" << endl;
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
	if (flags == 0)
		fds.erase(fd);
	else
		fds.insert(fd);
}

uint16_t AMQPInterface::onNegotiate(AMQP::TcpConnection* connection, uint16_t interval)
{
	//cout << "AMQP negotiate " << interval << endl;
	heartbeat_interval = interval - 2;
	auto now = std::chrono::steady_clock::now();
	next_heartbeat = now + std::chrono::duration<long int>(heartbeat_interval);
	return interval;
}

#endif
