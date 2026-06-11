#pragma once

#include "Client.hpp"

#include <map>
#include <vector>
#include <poll.h>
#include <memory>

class Server
{
public:
	Server();
	~Server();

	bool	setup(int port);
	void	run();

private:
	Server(const Server&);
	Server& operator=(const Server&);

	int							_server_fd;
	std::vector<struct pollfd>	_poll_fds;
	std::map<int, Client* >		_clients;

	void	accept_connection();
	void	rebuild_poll_array();
	void	handle_client(int fd);
	void	remove_client(int fd);

	static	bool set_nonblocking(int fd);
};
