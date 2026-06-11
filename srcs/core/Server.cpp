#include "Server.hpp"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// ─────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────

Server::Server() : _server_fd(-1) {}

Server::~Server()
{
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
        std::cerr << "fcntl O_NONBLOCK failed on fd=" << fd
                  << ": " << strerror(errno) << "\n";
        return false;
	}
	return true;    
}

bool	Server::setup(int port)
{
    
}

void	Server::run();
void	Server::accept_connection();
void	Server::rebuild_poll_array();
void	Server::handle_client(int fd);
void	Server::remove_client(int fd);



