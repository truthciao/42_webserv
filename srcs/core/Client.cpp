#include "Client.hpp"

#include <iostream>
#include <sys/socket.h>
#include <errno.h>
#include <cstring>
#include <sstream>

// ─────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────

Client::Client(int fd)
	: _fd(fd)
	, _state(READING)
	, _write_offset(0)
	, _request_complete(false)
{
	_read_buf.reserve(BUFFER_SIZE);
}

Client::~Client()
{
	if (_fd >= 0)
		close(_fd);
}


// ─────────────────────────────────────────────
// Reading phase  (state == READING, poll watches POLLIN)
// ─────────────────────────────────────────────

bool	Client::read_from_socket()
{
	char buffer[1024] = {0};

	while(true)
	{
		ssize_t bytes_read = recv(_fd, buffer, sizeof(buffer), 0);

		if (bytes_read > 0)
		{
			_read_buf.append(buffer, bytes_read);
			if (_read_buf.find("\r\n\r\n") != std::string::npos)
			{
				_request_complete = true;
				break;
			}
		}
		else if (bytes_read == 0)
		{
			std::cout << "[-] Client fd=" << _fd << " closed connection during read\n";
			_state = CLOSING;
			return false;
		}
		else if (bytes_read < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			std::cerr << "[-] Read error on fd=" << _fd << ": " << strerror(errno) << std::endl;
			_state = CLOSING;
			return false;
		}
	}
	return true;
}

// ─────────────────────────────────────────────
// Transition: READING → WRITING
// Called once we have a complete request in read_buf_.
// ─────────────────────────────────────────────
void	Client::prepare_reponse()
{
	std::cout << "\n[Request from fd=" << _fd << "]\n" << _read_buf << "\n";

	const std::string body = "<html><body><h1>Hello WebServ!</h1></body></html>";
	std::ostringstream oss;
	oss << body.size();
	_write_buf =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: " + oss.str() + "\r\n"
		"\r\n"
		+ body;

	_write_offset = 0;
	_state = WRITING;
}

// ─────────────────────────────────────────────
// Writing phase  (state == WRITING, poll watches POLLOUT)
// ─────────────────────────────────────────────

bool	Client::write_to_socket()
{
	while (_write_offset < _write_buf.size())
	{
		ssize_t bytes_written = send(
			_fd,
			_write_buf.c_str() + _write_offset,
			_write_buf.size() - _write_offset,
			0
		);

		if (bytes_written > 0)
			_write_offset += bytes_written;
		else if (bytes_written == 0)
			break;
		else
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return true;
			std::cerr << "Write error on fd=" << _fd << ": " << strerror(errno) << std::endl;
			_state = CLOSING;
			return false;
		}
	}

	std::cout << "[+] Response sent to fd=" << _fd << ", closing\n";
	_state = CLOSING;
	return false;
}
