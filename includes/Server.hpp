#pragma once

#include "Client.hpp"
#include "Config.hpp"

#include <map>
#include <vector>
#include <poll.h>
#include <memory>

enum CgiFdType
{
	CGI_FD_STDIN,
	CGI_FD_STDOUT
};

struct CgiFdInfo
{
	int			client_fd;
	CgiFdType	type;
};

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
	std::map<int, CgiFdInfo>			_cgi_fds;

	bool	setup_one_listener(const ServerConfig& server_cfg);
	void	accept_connection(int listen_fd);
	void	rebuild_poll_array();
	void	handle_client(int fd);
	void	remove_client(int fd);
	void	handle_cgi_fd(int fd, short revents);

	bool	is_listening_fd(int fd) const;
	static	bool set_nonblocking(int fd);
};
