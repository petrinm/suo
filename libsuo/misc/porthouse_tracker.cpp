#ifdef SUO_SUPPORT_AMQP

#include "porthouse_tracker.hpp"

#include <iostream>
#include <memory>
#include <algorithm> // max

#include "json.hpp"

using namespace std;
using namespace suo;

PorthouseTracker::Config::Config() {
	amqp_url = "amqp://guest:guest@localhost/";
	target_name = "";
	center_frequency = 437.5e6;
}

PorthouseTracker::PorthouseTracker(const Config& conf_) :
	conf(conf_),
	connection(this, AMQP::Address(conf.amqp_url)),
	channel(&connection),
	heartbeat_interval(30)
{

	channel.onError([](const char *message) {
		cout << "AMQP channel error: " << message << endl;
	});

	channel.declareQueue(AMQP::exclusive)
	.onSuccess([this](const std::string& name, uint32_t messagecount, uint32_t consumercount) {
		channel.bindQueue("tracking", name, "target.position");
		channel.consume(name)
		.onReceived([this](const AMQP::Message& message, uint64_t deliveryTag, bool redelivered) {
			message_callback(message, deliveryTag, redelivered);
		});
	});
	
	auto now = std::chrono::steady_clock::now();
	next_heartbeat = now + std::chrono::duration<long int>(heartbeat_interval);

	// TODO: Reconnect automatically after disconnection

}

void PorthouseTracker::tick(Timestamp _now)
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

/*
 */
void PorthouseTracker::message_callback(const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
{
	(void)deliveryTag;
	(void)redelivered;

	//cerr << "message_callback " << message.exchange() << " " << message.routingkey() << endl;
	//cerr << string_view(message.body(), message.bodySize()) << endl;

	// Parse JSON object from the message
	nlohmann::json dict;
	try {
		dict = nlohmann::json::parse(message.body(), message.body() + message.bodySize());
	}
	catch (const nlohmann::json::exception &e) {
		cerr << "Failed to parse JSON " << e.what() << endl;
		return;
	}

	// Filtering
	if (!conf.target_name.empty() && dict["target"] != conf.target_name)
		return;

	// Calculate Doppler shift
	float relative_velocity = dict["range_rate"]; // Must be in [m/s]
	float doppler = conf.center_frequency * (relative_velocity / 299792458.0);

#if 0
	cout << "Relative Velocity" << setprecision(2) << relative_velocity << " m/s; ";
	cout << "Doppler shift" << setprecision(1) << doppler << " Hz; ";
	cout << "Uplink " << setprecision(3) << conf.center_frequency + doppler) * 1e-6 << " MHz; ";
	cout << "Downlink" << setprecision(3) << (conf.center_frequency - doppler) * 1e-6 << " MHz" << endl;
#endif

	setUplinkFrequency.emit(conf.center_frequency + doppler);
	setDownlinkFrequency.emit(conf.center_frequency - doppler);
}

void PorthouseTracker::onAttached(AMQP::TcpConnection *connection)
{
	(void) connection;
	//cout << "AMQP Attached!" << endl;
}

void PorthouseTracker::onConnected(AMQP::TcpConnection *connection)
{
	(void)connection;
	cout << "AMQP Connected!" << endl;
}

void PorthouseTracker::onReady(AMQP::TcpConnection *connection)
{
	(void)connection;
	cout << "AMQP Ready!" << endl;
}

void PorthouseTracker::onError(AMQP::TcpConnection *connection, const char *message)
{
	(void)connection;
	cout << "AMQP Error!" << message << endl;
}

void PorthouseTracker::onClosed(AMQP::TcpConnection *connection)
{
	(void)connection;
	cout << "AMQP Closed!" << endl;
}

void PorthouseTracker::onLost(AMQP::TcpConnection *connection)
{
	(void)connection;
	cout << "AMQP Lost!" << endl;
}

void PorthouseTracker::onDetached(AMQP::TcpConnection *connection)
{
	(void)connection;
	cout << "AMQP Detached!" << endl;
}

void PorthouseTracker::monitor(AMQP::TcpConnection *connection, int fd, int flags)
{
	(void)connection;
	//cout << "AMQP Monitor " << fd << " " << flags << endl;

	if (flags == 0)
		fds.erase(fd);
	else 
		fds.insert(fd);
}

uint16_t PorthouseTracker::onNegotiate(AMQP::TcpConnection* connection, uint16_t interval)
{
	//cout << "AMQP negotiate " << interval << endl;
	heartbeat_interval = interval - 2;
	auto now = std::chrono::steady_clock::now();
	next_heartbeat = now + std::chrono::duration<long int>(heartbeat_interval);
	return interval;
}

#endif
