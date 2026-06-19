#include "Server.hpp"
#include "Logger.hpp"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <arpa/inet.h>

extern sig_atomic_t g_running;

// ─────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────

Server::Server() {}

Server::~Server()
{
    for (std::map<int, Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        delete it->second;
    }
	_clients.clear();

	for (std::map<int, const ServerConfig*>::iterator it = _listen_fd_to_config.begin();
		 it != _listen_fd_to_config.end(); ++it)
	{
		close(it->first);
	}
}

// ─────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────
bool Server::set_nonblocking(int fd)
{
	if(fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
	{
		LOG_SERVER_E() << "fcntl O_NONBLOCK failed on fd=" << fd
				  << ": " << strerror(errno);
		return false;
	}
	return true;
}

bool	Server::setup(const Config& config)
{
	_config	= config;

	const std::vector<ServerConfig>& servers = _config.getServers();
	if (servers.empty())
	{
		LOG_SERVER_E() << "No server blocks found in config";
		return false;
	}

	for (size_t i = 0; i < servers.size(); ++i)
	{
		if (!setup_one_listener(servers[i]))
			return false;
	}
	return true;
}

bool	Server::setup_one_listener(const ServerConfig& server_cfg)
{
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0)
	{
		LOG_SERVER_E() << "Socket creation failed: " << strerror(errno);
		return false;
	}

	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family 		= AF_INET;
	address.sin_port 		= htons(server_cfg.port);

	if (server_cfg.host.empty() || server_cfg.host == "0.0.0.0")
		address.sin_addr.s_addr = INADDR_ANY;
	else
		address.sin_addr.s_addr = inet_addr(server_cfg.host.c_str());

	if (bind(listen_fd, (struct sockaddr * )&address, sizeof(address)) < 0)
	{
		LOG_SERVER_E() << "Bind failed on " << server_cfg.host << ":" << server_cfg.port
					   << " - " << strerror(errno);
		close(listen_fd);
		return false;
	}

	if (listen(listen_fd, SOMAXCONN) < 0)
	{
		LOG_SERVER_E() << "Listen failed on port " << server_cfg.port;
		close(listen_fd);
		return false;
	}

	if (!set_nonblocking(listen_fd))
	{
		close(listen_fd);
		return false;
	}

	struct pollfd 	s_pfd;
	s_pfd.fd 		= listen_fd;
	s_pfd.events 	= POLLIN;
	s_pfd.revents 	= 0;
	_poll_fds.push_back(s_pfd);
	_listen_fd_to_config[listen_fd] = &server_cfg;

	LOG_SERVER_I() << "Server listening on " << server_cfg.host << ":" << server_cfg.port
				   << " (non-blocking mode)";
		return true;

}

bool	Server::is_listening_fd(int fd) const
{
	return _listen_fd_to_config.find(fd) != _listen_fd_to_config.end();
}

// ─────────────────────────────────────────────
// Main event loop
// ─────────────────────────────────────────────

void	Server::run()
{
	while (g_running)
	{
		int ready = poll(_poll_fds.data(), _poll_fds.size(), -1);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			LOG_SERVER_E() << "poll() failed: " << strerror(errno);
			break;
		}

		const size_t	n = _poll_fds.size();
		bool			need_rebuild = false;

		for (size_t i = n; i > 0; --i)
		{
			size_t idx = i - 1;
			if (_poll_fds[idx].revents == 0)
				continue;

			int	fd = _poll_fds[idx].fd;

			if (_poll_fds[idx].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (is_listening_fd(fd))
				{
					LOG_SERVER_E() << "Listening socket error on fd=" << fd;
					return ;
				}
				LOG_SERVER_E() << "[-] Error on fd=" << fd << ", closing";
				remove_client(fd);
				need_rebuild = true;
				continue;
			}

			if (is_listening_fd(fd) && _poll_fds[idx].revents & POLLIN)
			{
				accept_connection(fd);
				need_rebuild = true;
			}
			else
			{
				if (_poll_fds[idx].revents & (POLLIN | POLLOUT))
				{
					handle_client(fd);
					need_rebuild = true;
				}
			}
		}
		if (need_rebuild)
			rebuild_poll_array();
	}
}
// ─────────────────────────────────────────────
// Accept loop
// ─────────────────────────────────────────────

void	Server::accept_connection(int listen_fd)
{
	std::map<int, const ServerConfig*>::iterator cfg_it = _listen_fd_to_config.find(listen_fd);
	if (cfg_it == _listen_fd_to_config.end())
		return;

	const ServerConfig*	owning_config = cfg_it->second;

	while (true)
	{
		struct sockaddr_in 	client_addr;
		socklen_t 			client_addr_len = sizeof(client_addr);

		int	client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, & client_addr_len);
		if (client_fd < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			LOG_SERVER_E() << "Accept failed: " << strerror(errno);
			break;
		}

		if (!set_nonblocking(client_fd))
		{
			close(client_fd);
			continue;
		}

		_clients[client_fd] = new Client(client_fd, owning_config);

		LOG_SERVER_I() << "[+] New connection: fd=" << client_fd
				  << " on port " << owning_config->port
				  << " (total clients: " << _clients.size() << ")";
	}
}

void	Server::rebuild_poll_array()
{
	_poll_fds.clear();

	for (std::map<int, const ServerConfig*>::iterator it = _listen_fd_to_config.begin();
		 it != _listen_fd_to_config.end(); ++it)
	{
		struct pollfd 	s_pfd;
		s_pfd.fd 		= it->first;
		s_pfd.events 	= POLLIN;
		s_pfd.revents 	= 0;
		_poll_fds.push_back(s_pfd);
	}

	for (std::map<int, Client* >::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		int fd 			= it->first;
		Client* client 	= it->second;

		struct pollfd pfd;
		pfd.fd		= fd;
		pfd.revents	= 0;

		if (client->get_state() == READING)
			pfd.events = POLLIN;
		else
			pfd.events = POLLOUT;

		_poll_fds.push_back(pfd);
	}
}

// ─────────────────────────────────────────────
// Handle one client event
// ─────────────────────────────────────────────

void	Server::handle_client(int fd)
{
	std::map<int, Client* >::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	Client* client = it->second;

	if (client->get_state() == READING)
	{
		if (!client->read_from_socket())
		{
			remove_client(fd);
			return;
		}
	}
	else if (client->get_state() == WRITING)
	{
		if (!client->write_to_socket())
		{
			remove_client(fd);
			return;
		}
	}
	else
		remove_client(fd);
}

void	Server::remove_client(int fd)
{
	std::map<int, Client* >::iterator it = _clients.find(fd);
	if (it != _clients.end())
	{
		delete it->second;
		_clients.erase(it);
		LOG_SERVER_I() << "[-] Removed fd=" << fd
				  << " (active clients: " << _clients.size() << ")";
	}
}


