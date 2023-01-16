#pragma once

#ifdef SUO_SUPPORT_AMQP

#include <set>
#include <string>

#include "suo.hpp"

#include <amqpcpp.h>
#include <amqpcpp/linux_tcp.h>

namespace suo {

class PorthouseTracker : public AMQP::TcpHandler
{
public:
	struct Config
	{
		Config();

		/* AMQP server URL (for example: amqp://guest:guest@localhost/vhost) */
		std::string amqp_url;

		/* Name of the target for filtering */
		std::string target_name;

		/* Satellite center frequency in Hz */
		float center_frequency;
	};

	explicit PorthouseTracker(const Config &conf = Config());
	void tick(suo::Timestamp now);

	suo::Port<float> setUplinkFrequency;
	suo::Port<float> setDownlinkFrequency;

private:
	Config conf;
	std::set<int> fds;
	AMQP::TcpConnection connection;
	AMQP::TcpChannel channel;

	/* AMQP message callback */
	void message_callback(const AMQP::Message &message, uint64_t deliveryTag, bool redelivered);

	/* AMQP::TcpHandler callbacks */
	virtual void onAttached(AMQP::TcpConnection *connection) override;
	virtual void onConnected(AMQP::TcpConnection *connection) override;
	virtual void onReady(AMQP::TcpConnection *connection) override;
	virtual void onError(AMQP::TcpConnection *connection, const char *message) override;
	virtual void onClosed(AMQP::TcpConnection *connection) override;
	virtual void onLost(AMQP::TcpConnection *connection) override;
	virtual void onDetached(AMQP::TcpConnection *connection) override;
	virtual void monitor(AMQP::TcpConnection *connection, int fd, int flags) override;
};

}; // namespace suo

#endif