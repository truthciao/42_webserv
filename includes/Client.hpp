#pragma once

#include "Request.hpp"
#include "Response.hpp"

#include <string>
#include <unistd.h>
#include <fstream>

#define BUFFER_SIZE 4096
#define SEND_CHUNK_SIZE 65536

enum ClientState
{
	READING,
	WRITING,
	CLOSING,
};

enum	WriteStage
{
	WRITE_HEADER,
	WRITE_BODY,
	WRITE_DONE
};

class Client
{
public:
	explicit Client(int fd);
	~Client();

	bool	read_from_socket();
	bool	write_to_socket();
	void	prepare_reponse();

	int			get_fd()	const { return _fd; }
	ClientState	get_state()	const { return _state; }
	bool		request_complete() const { return _request.is_complete(); }

private:
	Client(const Client&);
	Client& operator=(const Client&);

	void	_process_data(const char* data, size_t len);

	bool	_send_header();
	bool	_send_file_body();

	int			_fd;
	ClientState	_state;

	Request		_request;
	Response	_response;

	std::string	_write_buf;
	size_t		_write_offset;

	std::ifstream	_file_stream;
	size_t			_file_remaining;
	WriteStage		_write_stage;
};


