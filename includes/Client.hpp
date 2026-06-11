#pragma once

#include <string>
#include <unistd.h>

#define BUFFER_SIZE 4096

enum ClientState
{
	READING,	
	WRITING,
	CLOSING,
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
	bool		request_complete() const { return _request_complete; }

private:
	Client(const Client&);
	Client& operator=(const Client&);

	int			_fd;
	ClientState	_state;

	std::string	_read_buf;
	std::string	_write_buf;
	size_t		_write_offset;

	bool		_request_complete;

	void		close_fd();
};


