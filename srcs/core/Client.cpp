#include "Client.hpp"
#include "Logger.hpp"
#include "Router.hpp"

#include <iostream>
#include <sys/socket.h>
#include <errno.h>
#include <cstring>
#include <sstream>

// ─────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────

Client::Client(int fd, const ServerConfig* server_config)
	: _fd(fd)
	, _state(READING)
	, _server_config(server_config)
{}

Client::~Client()
{
	if (_file_stream.is_open())
		_file_stream.close();

	for (std::deque<PendingResponse*>::iterator it = _response_queue.begin();
		 it != _response_queue.end(); ++it)
		 delete *it;

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

	while(true)
	{
		if (_request.has_error())
		{
			LOG_CLIENT_E() << "[-] Parse error on fd=" << _fd;
			_state = CLOSING;
			return ;
		}

		if (!_request.is_complete())
			break;

		LOG_CLIENT_D() << "Request read complete!";

		prepare_reponse();

		std::string leftover = _request.take_leftover();
		_request.reset();

		if (leftover.empty())
			break;

		_request.feed(leftover.c_str(), leftover.size());
	}
}


// ─────────────────────────────────────────────
// Transition: READING → WRITING
// ─────────────────────────────────────────────

void	Client::prepare_reponse()
{
	// _request.print();
	// LOG_CLIENT_D() << "Body content: " << _request.get_body();

	PendingResponse* pr = new PendingResponse();

	LocationConfig	matched_location;
	bool	has_location = Router::match(*_server_config, _request.get_uri(), matched_location);

	bool	is_file_response;
	if (!has_location)
		is_file_response = _response.build_no_location();
	else
		is_file_response = _response.build(_request.get_method(), _request.get_uri(),
											*_server_config, matched_location);

	pr->write_buf		= _response.get_raw();
	pr->write_offset	= 0;
	pr->is_file			= is_file_response;
	pr->write_stage		= WRITE_HEADER;

	if (is_file_response)
	{
		pr->file_path		= _response.get_file_path();
		pr->file_remaining	= _response.get_file_size();
	}

	_response_queue.push_back(pr);

	LOG_CLIENT_D() << "Prepare reponse complete! Queue size = " << _response_queue.size();

	_state = WRITING;
}

// ─────────────────────────────────────────────
// Writing phase  (state == WRITING, poll watches POLLOUT)
// ─────────────────────────────────────────────

bool	Client::write_to_socket()
{
	if (_response_queue.empty())
	{
		_state = CLOSING;
		return false;
	}

	PendingResponse* pr = _response_queue.front();

	if (pr->write_stage == WRITE_HEADER)
	{
		if (!_send_header(pr))
			return (_state != CLOSING) ? true : false;
		LOG_CLIENT_D() << "Send header complete!";

		if (pr->is_file)
		{
			_file_stream.open(pr->file_path.c_str(), std::ios::binary);
			if (!_file_stream.is_open())
			{
				LOG_CLIENT_E() << "[-] Failed to open file for streaming: "
							<< _response.get_file_path();
				_state = CLOSING;
				return false;
			}
			pr->write_stage = WRITE_BODY;
		}
		else
			pr->write_stage = WRITE_DONE;
	}

	if (pr->write_stage == WRITE_BODY)
	{
		if (!_send_file_body(pr))
			return (_state != CLOSING) ? true : false;
		LOG_CLIENT_D() << "Send body complete!";
		pr->write_stage = WRITE_DONE;
	}

	delete pr;
	_response_queue.pop_front();

	if (!_response_queue.empty())
		return true;

	LOG_CLIENT_I() << "All responses sent to fd=" << _fd << ", closing";

	_state = CLOSING;
	return false;
}

bool	Client::_send_header(PendingResponse* pr)
{
	while (pr->write_offset < pr->write_buf.size())
	{
		size_t	remaining	= pr->write_buf.size() - pr->write_offset;
		size_t	to_send		= (remaining < SEND_CHUNK_SIZE) ? remaining : SEND_CHUNK_SIZE;

		ssize_t bytes_written = send(
			_fd,
			pr->write_buf.c_str() + pr->write_offset,
			to_send,
			0
		);

		if (bytes_written > 0)
			pr->write_offset += bytes_written;
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

bool	Client::_send_file_body(PendingResponse* pr)
{
	char chunk[SEND_CHUNK_SIZE];

	while (pr->file_remaining > 0 || pr->write_offset < pr->write_buf.size())
	{
		if (pr->write_offset >= pr->write_buf.size())
		{
			size_t to_read = (pr->file_remaining < SEND_CHUNK_SIZE) ? pr->file_remaining : SEND_CHUNK_SIZE;

			_file_stream.read(chunk, static_cast<std::streamsize>(to_read));
			std::streamsize got = _file_stream.gcount();

			if (got <= 0)
			{
				LOG_CLIENT_E() << "[-] File read error on file stream";
				_state = CLOSING;
				return false;
			}

			pr->write_buf.assign(chunk, static_cast<size_t>(got));
			pr->write_offset = 0;
			pr->file_remaining -= static_cast<size_t>(got);
		}

		ssize_t bytes_written = send(
			_fd,
			pr->write_buf.c_str() + pr->write_offset,
			pr->write_buf.size() - pr->write_offset,
			MSG_NOSIGNAL
		);

		if (bytes_written > 0)
			pr->write_offset += bytes_written;
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
