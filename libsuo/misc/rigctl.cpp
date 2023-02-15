#include "rigctl.hpp"
#include "registry.hpp"

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <sstream>

using namespace std;
using namespace suo;

RigCtl::Config::Config() {
	port = 4532;
}

RigCtl::RigCtl(const Config& conf) {
	//

	listening_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listening_fd < 0)
		throw runtime_error("Failed to create listening socket for rigctl");

	struct sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(conf.port);
	inet_pton(AF_INET, "0.0.0.0", &hint.sin_addr);

	cout << "Binding socket to sockaddr..." << endl;
	if (bind(listening_fd, (struct sockaddr *)&hint, sizeof(hint)) < 0)
		throw runtime_error("Failed to bind rigctl listening socket to conf.port");

	if (listen(listening_fd, SOMAXCONN) < 0)
		throw runtime_error("Failed to start listening rigctl listening socket");

}

RigCtl::~RigCtl()
{
	// Close all sockets
	close(listening_fd);
	for (int fd : client_fds)
		close(fd);
}


void RigCtl::tick(Timestamp now) {
	(void)now;

	int nfds = listening_fd;
	fd_set rset;
	FD_ZERO(&rset);
	FD_SET(listening_fd, &rset);

	for (int fd : client_fds)
	{
		FD_SET(fd, &rset);
		nfds = max(nfds, fd);
	}

	struct timeval timeout = {0, 0}; // Non-blocking select
	if (select(nfds, &rset, NULL, NULL, &timeout) > 0)
	{

		if (FD_ISSET(listening_fd, &rset))
		{
			sockaddr_in client;
			socklen_t clientSize = sizeof(client);
			int clientSocket = accept(listening_fd, (struct sockaddr *)&client, &clientSize);
			client_fds.insert(clientSocket);
			cout << "Rigctl: Client " << inet_ntoa(client.sin_addr) << ":" << client.sin_port << " connected" << endl;
		}

		char buffer[1024];

		for (int client_fd : client_fds)
		{
			if (FD_ISSET(client_fd, &rset))
			{
				int bytesRecv = recv(client_fd, buffer, size(buffer), 0);
				if (bytesRecv < 0) {
					if (errno == ECONNRESET) {
						client_fds.erase(client_fd);
						cout << "Rigctl: Client disconnected" << endl;
					}
				}

				string response;
				handleCommand(string(buffer, 0, bytesRecv), response);
				if (response.empty() == false)
					send(client_fd, response.c_str(), response.size(), 0);
			}
		}
	}
}

void RigCtl::handleCommand(const string& command, string& response_str)
{
	cout << "Rigctl: received '" << command << "'" << endl;
	stringstream response;

	if (command == "dump_state")
	{
		response <<  "0\n1\n2\n150000.000000 1500000000.000000 0x1ff -1 -1 0x10000003 0x3\n";
		response << "0 0 0 0 0 0 0\n";
		response << "0 0 0 0 0 0 0\n";
		response << "0x1ff 1\n";
		response << "0x1ff 0\n";
		response << "0 0\n";
		response << "0x1e 2400\n";
		response << "0x2 500\n";
		response << "0x1 8000\n";
		response << "0x1 2400\n";
		response << "0x20 15000\n";
		response << "0x20 8000\n";
		response << "0x40 230000\n";
		response << "0 0\n";
		response << "9990\n";
		response << "9990\n";
		response << "10000\n";
		response << "0\n";
		response << "10\n";
		response << "10 20 30\n";
		response << "0xffffffffffffffff\n";
		response << "0xffffffffffffffff\n";
		response << "0xfffffffff7ffffff\n";
		response << "0xffffffff83ffffff\n";
		response << "0xffffffffffffffff\n";
		response << "0xffffffffffffffbf\n";
	}
	else if (command == "f") // Get frequency
	{
		response << 437e6 << "\n";
	}
	else if (command == "F") // Set frequency
	{
		response << "RPRT 0\n";
	}
	else if (command == "v") // Get VFO
	{
		response << "VFOA\n";
	}
	else if (command == "v") // Set VFO
	{
		response << "RPRT 0\n";
	}

	response_str = response.str();
}

Block *createRigCtl(const Kwargs &args)
{
	return new RigCtl();
}

static Registry registerRigCtl("RigCtl", &createRigCtl);
