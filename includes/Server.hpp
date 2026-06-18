#pragma once

#include "Client.hpp"
#include "Config.hpp"

#include <map>
#include <vector>
#include <poll.h>
#include <memory>

class Server
{
public:
	Server();
	~Server();

	bool	setup(const Config& config);
	void	run();

private:
	Server(const Server&);
	Server& operator=(const Server&);

	std::map<int, const ServerConfig*>	_listen_fd_to_config;
	std::vector<struct pollfd>			_poll_fds;
	std::map<int, Client* >				_clients;
	Config								_config;

	bool	setup_one_listener(const ServerConfig& server_cfg);
	void	accept_connection(int listen_fd);
	void	rebuild_poll_array();
	void	handle_client(int fd);
	void	remove_client(int fd);

	bool	is_listening_fd(int fd) const;

	static	bool set_nonblocking(int fd);
};
