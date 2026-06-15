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
	, _file_remaining(0)
	, _write_stage(WRITE_HEADER)
{}

Client::~Client()
{
	if (_file_stream.is_open())
		_file_stream.close();
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
		LOG_CLIENT_D() << "Request read complete!";

		prepare_reponse();

		std::string leftover = _request.take_leftover();
		_request.reset();

		if (!leftover.empty())
			_process_data(leftover.c_str(), leftover.size());
	}
}


// ─────────────────────────────────────────────
// Transition: READING → WRITING
// ─────────────────────────────────────────────

void	Client::prepare_reponse()
{
	_request.print();
	LOG_CLIENT_I() << "Body content: " << _request.get_body();

	bool is_file_response = _response.build(_request.get_uri(), "./www");

	_write_buf = _response.get_raw();
	_write_offset = 0;

	if (is_file_response)
	{
		_file_stream.open(_response.get_file_path().c_str(), std::ios::binary);
		if (!_file_stream.is_open())
		{
			LOG_CLIENT_E() << "[-] Failed to open file for streaming: "
						   << _response.get_file_path();
			_state = CLOSING;
			return;
		}
		_file_remaining = _response.get_file_size();
		_write_stage = WRITE_HEADER;
	}
	else
		_write_stage = WRITE_DONE;

	LOG_CLIENT_D() << "Prepare reponse complete!";

	_state = WRITING;
}

// ─────────────────────────────────────────────
// Writing phase  (state == WRITING, poll watches POLLOUT)
// ─────────────────────────────────────────────

bool	Client::write_to_socket()
{
	if (_write_stage == WRITE_HEADER)
	{
		if (!_send_header())
			return (_state != CLOSING) ? true : false;
		LOG_CLIENT_D() << "Send header complete!";
		_write_stage = (_file_stream.is_open()) ? WRITE_BODY : WRITE_DONE;
	}

	if (_write_stage == WRITE_BODY)
	{
		if (!_send_file_body())
			return (_state != CLOSING) ? true : false;
		LOG_CLIENT_D() << "Send body complete!";
		_write_stage = WRITE_DONE;
	}

	LOG_CLIENT_I() << "[+] Response sent to fd=" << _fd << ", closing";
	_state = CLOSING;
	return false;
}

bool	Client::_send_header()
{
	while (_write_offset < _write_buf.size())
	{
		size_t	remaining	= _write_buf.size() - _write_offset;
		size_t	to_send		= (remaining < SEND_CHUNK_SIZE) ? remaining : SEND_CHUNK_SIZE;

		ssize_t bytes_written = send(
			_fd,
			_write_buf.c_str() + _write_offset,
			to_send,
			0
		);

		if (bytes_written > 0)
			_write_offset += bytes_written;
		else if (bytes_written == 0)
			break;
		else
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return false;
			LOG_CLIENT_E() << "Write error on fd=" << _fd << ": " << strerror(errno);
			_state = CLOSING;
			return false;
		}
	}
	return true;
}

bool	Client::_send_file_body()
{
	char chunk[SEND_CHUNK_SIZE];

	while (_file_remaining > 0 || _write_offset < _write_buf.size())
	{
		if (_write_offset >= _write_buf.size())
		{
			size_t to_read = (_file_remaining < SEND_CHUNK_SIZE) ? _file_remaining : SEND_CHUNK_SIZE;

			_file_stream.read(chunk, static_cast<std::streamsize>(to_read));
			std::streamsize got = _file_stream.gcount();

			if (got <= 0)
			{
				LOG_CLIENT_E() << "[-] File read error on file stream";
				_state = CLOSING;
				return false;
			}

			_write_buf.assign(chunk, static_cast<size_t>(got));
			_write_offset = 0;
			_file_remaining -= static_cast<size_t>(got);
		}

		ssize_t bytes_written = send(
			_fd,
			_write_buf.c_str() + _write_offset,
			_write_buf.size() - _write_offset,
			MSG_NOSIGNAL
		);

		if (bytes_written > 0)
			_write_offset += bytes_written;
		else if (bytes_written == 0)
			break;
		else
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
    			LOG_CLIENT_D() << "Socket buffer full on fd=" << _fd << ", will try again later.";
				return false;
			}
			LOG_CLIENT_E() << "Write error on fd=" << _fd << ": " << strerror(errno);
			_state = CLOSING;
			return false;
		}
	}
	_file_stream.close();
	return true;
}
