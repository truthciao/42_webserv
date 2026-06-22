#include "Client.hpp"
#include "Logger.hpp"
#include "Router.hpp"
#include "MultipartParser.hpp"
#include "Autoindex.hpp"

#include <iostream>
#include <sys/socket.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────

Client::Client(int fd, const ServerConfig* server_config)
	: _fd(fd)
	, _state(READING)
	, _server_config(server_config)
	, _cgi(NULL)
{}

Client::~Client()
{
	if (_file_stream.is_open())
		_file_stream.close();

	for (std::deque<PendingResponse*>::iterator it = _response_queue.begin();
		 it != _response_queue.end(); ++it)
		 delete *it;

	delete _cgi;
	_cgi = NULL;

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
// prepare_reponse(): CGI or static file
// ─────────────────────────────────────────────

void	Client::prepare_reponse()
{
	_request.print();
	LOG_CLIENT_D() << "Body content: " << _request.get_body();

	LocationConfig	matched_loc;
	bool	has_location = Router::match(*_server_config, _request.get_uri(), matched_loc);

	if (!has_location)
	{
		PendingResponse* pr = new PendingResponse();

		_response.build_no_location();
		pr->write_buf		= _response.get_raw();
		pr->write_offset	= 0;
		pr->is_file			= true;
		pr->write_stage		= WRITE_HEADER;
		_response_queue.push_back(pr);
		_state = WRITING;
		return;
	}

	std::string script_path, interpreter, cwd;
	if (_detect_cgi(_request.get_uri(), matched_loc, script_path, interpreter, cwd))
	{
		_start_cgi(script_path, interpreter, cwd, matched_loc);
		return;
	}

	PendingResponse* pr = new PendingResponse();
	bool is_file_response = _response.build(_request.get_method(), _request.get_uri(),
											*_server_config, matched_loc);
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
// CGI：detecting if need CGI
// ─────────────────────────────────────────────

bool	Client::_detect_cgi(	const std::string& uri,
								const LocationConfig& loc,
								std::string& out_script_path,
								std::string& out_interpreter,
								std::string& out_cwd)	const
{
	if (loc.cgi_ext_path.empty())
		return false;

	std::string	path = uri;
	size_t	q = path.find('?');
	if (q != std::string::npos)
		path = path.substr(0, q);

	for (std::map<std::string, std::string>::const_iterator it = loc.cgi_ext_path.begin();
		 it != loc.cgi_ext_path.end(); ++it)
	{
		const std::string& ext = it->first;

		if (path.size() >= ext.size()
			&& path.compare(path.size() - ext.size(), ext.size(), ext) == 0)
		{
			out_script_path = loc.root + path;
			out_interpreter = it->second;

			size_t slash = out_script_path.find_last_not_of('/');
			out_cwd = (slash == std::string::npos) ? "." : out_script_path.substr(0, slash);

			return true;
		}
	}
	return false;
}

// ─────────────────────────────────────────────
// CGI：构造环境变量
// ─────────────────────────────────────────────

std::map<std::string, std::string>
Client::_build_cgi_env(	const std::string& script_path,
						const LocationConfig& loc)	const
{
	(void)loc;
	std::map<std::string, std::string> env;

	const std::map<std::string, std::string>& headers = _request.get_headers();

	// ── 标准 CGI/1.1 元变量 ──
	env["GATEWAY_INTERFACE"] = "CGI/1.1";
	env["SERVER_PROTOCOL"]   = _request.get_version();
	env["SERVER_SOFTWARE"]   = "webserv/1.0";
	env["REQUEST_METHOD"]    = _request.get_method();

	// ── 服务器信息（从 config 读取）──
	{
		std::ostringstream	port_oss;
		port_oss << _server_config->port;
		env["SERVER_PORT"] = port_oss.str();
	}
	env["SERVER_NAME"] = _server_config->server_names.empty()
						 ? "localhost"
						 : _server_config->server_names[0];

	std::string	raw_uri = _request.get_uri();
	std::string	path_info;
	std::string	query_str;

	size_t	q = raw_uri.find('?');
	if (q != std::string::npos)
	{
		path_info	= raw_uri.substr(0, q);
		query_str	= raw_uri.substr(q + 1);
	}
	else
		path_info = raw_uri;

	env["PATH_INFO"]       = path_info;
	env["QUERY_STRING"]    = query_str;
	env["SCRIPT_NAME"]     = path_info;
	env["SCRIPT_FILENAME"] = script_path;

	// ── Body 相关 ──
	std::map<std::string, std::string>::const_iterator it;

	{
		std::ostringstream cl;
		cl << _request.get_body().size();
		env["CONTENT_LENGTH"] = cl.str();
	}

	it = headers.find("content-type");
	if (it != headers.end())
		env["CONTENT_TYPE"] = it->second;

	// ── HTTP_* 转换所有请求头 ──
	for (it = headers.begin(); it != headers.end(); ++it)
	{
		std::string key = "HTTP_";
		for (size_t i = 0; i < it->first.size(); ++i)
		{
			char c = it->first[i];
			key += (c == '-') ? '_' : static_cast<char>(std::toupper(c));
		}
		if (key == "HTTP_CONTENT_LENGTH" || key == "HTTP_CONTENT_TYPE")
			continue;
		env[key] = it->second;
	}

	// php-cgi 安全模式需要这个变量
	env["REDIRECT_STATUS"] = "200";

	return env;
}

// ─────────────────────────────────────────────
// CGI：Start
// ─────────────────────────────────────────────

void	Client::_start_cgi(	const std::string& script_path,
					const std::string& interpreter,
					const std::string& cwd,
					const LocationConfig& loc)
{
	if (access(script_path.c_str(), F_OK | X_OK) != 0)
	{
		_enqueue_raw_response(
			"HTTP/1.1 404 Not Found\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 43\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<html><body>404 CGI not found</body></html>");
		_state = WRITING;
    	LOG_CGI_W() << "Script not found or not executable: " << script_path;
		return;
	}

	delete	_cgi;
	_cgi = new CgiHandler();

	std::map<std::string, std::string> env = _build_cgi_env(script_path, loc);

	if (!_cgi->start(script_path, interpreter, env, _request.get_body(), cwd))
	{
		LOG_CGI_E() << "Failed to start CGI for fd=" << _fd;
		delete _cgi;
		_cgi = NULL;
		_enqueue_raw_response(
			"HTTP/1.1 500 Internal Server Error\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 38\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<html><body>500 CGI failed</body></html>");
		_state = WRITING;
		return;
	}

	if (!_cgi->stdin_done())
		_state = CGI_WRITING_STDIN;
	else
		_state = CGI_READING_STDOUT;

	LOG_CLIENT_D() << "CGI started for fd=" << _fd
				   << " script=" << script_path;
}

// ─────────────────────────────────────────────
// CGI：对外接口（供 Server 的 poll 分发）
// ─────────────────────────────────────────────

int		Client::get_cgi_stdin_fd()	const
{
	if (_cgi && !_cgi->stdin_done())
		return _cgi->get_stdin_fd();
	return -1;
}

int		Client::get_cgi_stdout_fd()	const
{
	if (_cgi && !_cgi->stdou_done())
		return _cgi->get_stdout_fd();
	return -1;
}

void	Client::handle_cgi_stdin_writable()
{
	if(!_cgi)
		return;

	_cgi->write_to_stdin();

	if (_cgi->stdin_done())
		_state = CGI_READING_STDOUT;
}
void	Client::handle_cgi_stdout_readable()
{
	if(!_cgi)
		return;

	bool more = _cgi->read_from_stdout();

	if (!more)
	{
		_cgi->check_child_status();
		_finish_cgi();
	}
}

void	Client::check_cgi_timeout()
{
	if (!_cgi || _cgi->get_state() != CGI_RUNNING)
		return;

	_cgi->check_child_status();

	if (_cgi->get_state() == CGI_TIMEOUT)
	{
		LOG_CGI_W() << "CGI timeout on fd=" << _fd;

		std::ostringstream body;
		body << "<html><body><h1>504 Gateway Timeout</h1></body></html>";
		std::string b = body.str();

		std::ostringstream h;
		h	<< "HTTP/1.1 504 Gateway Timeout\r\n"
			<< "Content-Type: text/html\r\n"
			<< "Content-Length: " << b.size() << "\r\n"
			<< "Connection: close\r\n\r\n";

		_enqueue_raw_response(h.str() + b);
		_state = WRITING;
		delete _cgi;
		_cgi = NULL;
	}
}

// ─────────────────────────────────────────────
// CGI 输出 -> 构造 HTTP 响应
// ─────────────────────────────────────────────

void	Client::_finish_cgi()
{
	if (!_cgi)
		return;

	if (_cgi->get_state() == CGI_ERROR || _cgi->get_state() == CGI_TIMEOUT)
	{
		std::string body = "<html><body><h1>502 Bad Gateway</h1></body></html>";
		std::ostringstream h;
		h << "HTTP/1.1 502 Bad Gateway\r\n"
		  << "Content-Type: text/html\r\n"
		  << "Content-Length: " << body.size() << "\r\n"
		  << "Connection: close\r\n\r\n";
		_enqueue_raw_response(h.str() + body);
		delete _cgi;
		_cgi = NULL;
		_state = WRITING;
		return;
	}

	const std::string& raw_output = _cgi->get_output();

	size_t	sep = raw_output.find("\r\n\r\n");

	std::string cgi_headers;
	std::string cgi_body;

	if (sep != std::string::npos)
	{
		cgi_headers	= raw_output.substr(0, sep);
		cgi_body	= raw_output.substr(sep + 4);
	}
	else
	{
		LOG_CGI_W() << "CGI output missing header separator, treating as body";
		cgi_body = raw_output;
	}

	std::string content_type = "text/html";
	std::string status_line  = "200 OK";
	std::ostringstream extra_headers;

	std::istringstream	hs(cgi_headers);
	std::string	line;
	while (std::getline(hs, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue;

		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		std::string	key = line.substr(0, colon);
		std::string	val = line.substr(colon + 1);

		size_t start = val.find_first_not_of(' ');
		if (start != std::string::npos)
			val = val.substr(start);

		std::string lower_key = key;
		std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

		if (lower_key == "content-type")
			content_type = val;
		else if (lower_key == "status")
			status_line = val;
		else
			extra_headers << key << ": " << val << "\r\n";
	}

	std::ostringstream h;
	h	<< "HTTP/1.1 " << status_line << "\r\n"
		<< "Content-Type: " << content_type << "\r\n"
		<< "Content-Length: " << cgi_body.size() << "\r\n"
		<< extra_headers.str()
		<< "Connection: close\r\n"
		<< "\r\n";

	_enqueue_raw_response(h.str() + cgi_body);
	_state = WRITING;

	LOG_CLIENT_D() << "CGI finished for fd=" << _fd
				   << ", output size=" << cgi_body.size();

	delete _cgi;
	_cgi = NULL;
}

void	Client::_enqueue_raw_response(const std::string& raw)
{
	PendingResponse* pr = new PendingResponse();
	pr->write_buf		= raw;
	pr->write_offset	= 0;
	pr->is_file			= false;
	pr->write_stage		= WRITE_HEADER;
	_response_queue.push_back(pr);
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



