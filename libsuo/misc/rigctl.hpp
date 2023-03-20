#pragma once

#include <suo.hpp>
#include <set>

namespace suo {

class RigCtl : public Block
{
public:

	struct Config
	{
		Config();

		/* TCP port the daemon will be listening (4532 by default) */
		unsigned int port;
	};

	explicit RigCtl(const Config &conf = Config());
	~RigCtl();

	void tick(Timestamp now);

	Port<float> setUplinkFrequency;
	Port<float> setDownlinkFrequency;

private:
	int listening_fd;
	std::set<int> client_fds;

	void handleCommand(const std::string &command, std::string &response);
};

}; // namespace suo
