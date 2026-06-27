#pragma once

#include "Request.hpp"
#include "CgiSession.hpp"
#include "Response.hpp"
#include "Config.hpp"
#include "Session.hpp"

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

	bool	_detect_cgi(	const std::string& uri,
							const LocationConfig& loc,
							ScriptInfo& out_script)	const;
	void	_start_cgi(	const ScriptInfo& script,
						const LocationConfig& loc);

	void	_deliver_cgi_result();
	void	_enqueue_raw_response(const std::string& raw, bool is_file = false);

	void	_handle_upload(const LocationConfig& loc);
	void 	_handle_upload_multipart(const LocationConfig& loc, const std::string& ct);
	void	_handle_upload_octet_stream(const LocationConfig& loc);
	void	_handle_delete(const LocationConfig& loc);
	void	_handle_autoindex(const std::string& uri, const LocationConfig& loc);

	void	_resolve_session();

	std::string			_session_id;
	int					_fd;
	ClientState			_state;
	const ServerConfig*	_server_config;
	Request				_request;
	Response			_response;
	CgiSession*			_cgi;

	std::deque<PendingResponse*> _response_queue;
	std::ifstream				_file_stream;

};


