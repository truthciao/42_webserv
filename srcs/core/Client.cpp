#include "Client.hpp"
#include "Logger.hpp"

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
{}

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
	char buffer[BUFFER_SIZE];

	while(true)
	{
		ssize_t bytes_read = recv(_fd, buffer, sizeof(buffer), 0);

		if (bytes_read > 0)
		{
			_process_data(buffer, static_cast<size_t>(bytes_read));
		}
		else if (bytes_read == 0)
		{
			LOG_CLIENT_I() << "[-] Client fd=" << _fd << " closed connection during read";
			_state = CLOSING;
			return false;
		}
		else if (bytes_read < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			LOG_CLIENT_E() << "[-] Read error on fd=" << _fd << ": " << strerror(errno);
			_state = CLOSING;
			return false;
		}
	}
	return true;
}

void	Client::_process_data(const char* data, size_t len)
{
	_request.feed(data, len);

	if (_request.has_error())
	{
		LOG_CLIENT_E() << "[-] Parse error on fd=" << _fd;
		_state = CLOSING;
		return ;
	}

	if (_request.is_complete())
	{
		prepare_reponse();

		std::string leftover = _request.take_leftover();
		_request.reset();

		if (!leftover.empty())
			_process_data(leftover.c_str(), leftover.size());
	}
}


// ─────────────────────────────────────────────
// Transition: READING → WRITING
// Called once we have a complete request in read_buf_.
// ─────────────────────────────────────────────

void	Client::prepare_reponse()
{
	// _request.print();
	_response.build(_request.get_uri(), "./www");

	_write_buf = _response.get_raw();
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
			LOG_CLIENT_E() << "Write error on fd=" << _fd << ": " << strerror(errno);
			_state = CLOSING;
			return false;
		}
	}

	LOG_CLIENT_I() << "[+] Response sent to fd=" << _fd << ", closing";
	_state = CLOSING;
	return false;
}
