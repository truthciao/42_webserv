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

extern sig_atomic_t g_running;

// ─────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────

Server::Server() : _server_fd(-1) {}

Server::~Server()
{
    for (std::map<int, Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        delete it->second;
    }
	_clients.clear();

	if (_server_fd >= 0)
		close(_server_fd);
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

bool	Server::setup(int port)
{
	_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_server_fd < 0)
	{
		LOG_SERVER_E() << "Socket creation failed: " << strerror(errno);
		return false;
	}

	int opt = 1;
	setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family 		= AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port 		= htons(port);

	if (bind(_server_fd, (struct sockaddr * )&address, sizeof(address)) < 0)
	{
		LOG_SERVER_E() << "Bind failed! Port might be in use.";
		close(_server_fd);
		return false;
	}

	if (listen(_server_fd, SOMAXCONN) < 0)
	{
		LOG_SERVER_E() << "Listen Failed!";
		close(_server_fd);
		return false;
	}

	if (!set_nonblocking(_server_fd))
	{
		close(_server_fd);
		return false;
	}

	struct pollfd s_pfd;
	s_pfd.fd 		= _server_fd;
	s_pfd.events 	= POLLIN;
	s_pfd.revents 	= 0;
	_poll_fds.push_back(s_pfd);

	LOG_SERVER_I() << "Server listening on port " << port << " (non-blocking mode)";
	return true;
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
			if (_poll_fds[idx].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (_poll_fds[idx].fd == _server_fd)
				{
					LOG_SERVER_E() << "Server socket error!";
					return ;
				}
				std::cout << "[-] Error on fd=" << _poll_fds[idx].fd << ", closing";
				remove_client(_poll_fds[idx].fd);
				need_rebuild = true;
				continue;
			}

			if (_poll_fds[idx].revents & (POLLIN | POLLOUT))
			{
				if (_poll_fds[idx].fd == _server_fd)
				{
					accept_connection();
					need_rebuild = true;
				}
				else
				{
					handle_client(_poll_fds[idx].fd);
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

void	Server::accept_connection()
{
	while (true)
	{
		struct sockaddr_in 	client_addr;
		socklen_t 			client_addr_len = sizeof(client_addr);

		int	client_fd = accept(_server_fd, (struct sockaddr *)&client_addr, & client_addr_len);
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

		_clients[client_fd] = new Client(client_fd);

		LOG_SERVER_I() << "[+] New connection: fd=" << client_fd
				  << " (total clients: " << _clients.size() - 1 << ")";
	}
}

void	Server::rebuild_poll_array()
{
	_poll_fds.clear();

	struct pollfd s_pfd;
	s_pfd.fd 		= _server_fd;
	s_pfd.events 	= POLLIN;
	s_pfd.revents 	= 0;
	_poll_fds.push_back(s_pfd);

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
				  << " (active clients: " << _clients.size() << ")\n";
	}
}



