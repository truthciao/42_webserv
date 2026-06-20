#pragma once

#include "Request.hpp"
#include "CgiHandler.hpp"
#include "Response.hpp"
#include "Config.hpp"

#include <string>
#include <deque>
#include <map>
#include <unistd.h>
#include <fstream>

#define BUFFER_SIZE 4096
#define SEND_CHUNK_SIZE 65536

enum ClientState
{
	READING,
	WRITING,
	CGI_WRITING_STDIN,
	CGI_READING_STDOUT,
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
	Client(int fd, const ServerConfig* server_config);
	~Client();

	bool	read_from_socket();
	bool	write_to_socket();
	void	prepare_reponse();

	int			get_fd()			const { return _fd; }
	ClientState	get_state()			const { return _state; }
	bool		request_complete()	const { return _request.is_complete(); }

	int		get_cgi_stdin_fd()	const;
	int		get_cgi_stdout_fd()	const;

	void	handle_cgi_stdin_writable();
	void	handle_cgi_stdout_readable();
	void	check_cgi_timeout();

private:
	Client(const Client&);
	Client& operator=(const Client&);

	void	_process_data(const char* data, size_t len);

	bool	_send_header(PendingResponse*);
	bool	_send_file_body(PendingResponse*);
	void	_start_next_response();

	bool	_detect_cgi(	const std::string& uri,
							const LocationConfig& loc,
							std::string& out_script_path,
							std::string& out_interpreter,
							std::string& out_cwd)	const;

	void	_start_cgi(	const std::string& script_path,
						const std::string& interpreter,
						const std::string& cwd,
						const LocationConfig& loc);

	std::map<std::string, std::string>	_build_cgi_env(	const std::string& script_path,
														const LocationConfig& loc)	const;

	void	_finish_cgi();
	void	_enqueue_raw_response(const std::string& raw);

	int					_fd;
	ClientState			_state;
	const ServerConfig*	_server_config;

	Request		_request;
	Response	_response;

	std::deque<PendingResponse*> _response_queue;
	std::ifstream				_file_stream;

	CgiHandler*	_cgi;
};


