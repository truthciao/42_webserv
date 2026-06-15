#pragma once

#include "Request.hpp"
#include "Response.hpp"

#include <string>
#include <deque>
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

struct PendingResponse
{
	std::string	write_buf;
	size_t		write_offset;

	bool		is_file;
	std::string	file_path;
	size_t		file_remaining;
	WriteStage	write_stage;

	PendingResponse()
		: write_offset(0)
		, is_file(false)
		, file_remaining(0)
		, write_stage(WRITE_HEADER)
	{}
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

	bool	_send_header(PendingResponse*);
	bool	_send_file_body(PendingResponse*);
	void	_start_next_response();

	int			_fd;
	ClientState	_state;

	Request		_request;
	Response	_response;

	std::deque<PendingResponse*> _response_queue;

	std::ifstream	_file_stream;
};


