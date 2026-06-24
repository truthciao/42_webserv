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
#include <sys/stat.h>


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
	LocationConfig	matched_loc;
	bool	has_location = Router::match(*_server_config, _request.get_uri(), matched_loc);

	if (!has_location)
	{
		bool is_file = _response.build_no_location(*_server_config);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		return;
	}

	std::string script_path, interpreter, cwd;
	if (_detect_cgi(_request.get_uri(), matched_loc, script_path, interpreter, cwd))
	{
		_start_cgi(script_path, interpreter, cwd, matched_loc);
		return;
	}
	
	if (!matched_loc.allow_methods.empty() &&
		matched_loc.allow_methods.find(_request.get_method()) == matched_loc.allow_methods.end())
	{
		bool is_file = _response.build_error(*_server_config, 405);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		return;
	}

	const std::string& method = _request.get_method();

	if (method == "DELETE")
	{
		_handle_delete(matched_loc);
		return;
	}

	if (method == "POST" && matched_loc.upload_enable)
	{
		_handle_upload(matched_loc);
		return;
	}

	bool is_file_response = _response.build(_request.get_method(), _request.get_uri(),
											*_server_config, matched_loc);
	if (_response.is_autoindex_needed())
	{
		if (matched_loc.autoindex)
			_handle_autoindex(_request.get_uri(), matched_loc);
		else
		{
			bool is_file = _response.build_error(*_server_config, 404);
			_enqueue_raw_response(_response.get_raw(), is_file);
			_state = WRITING;
		}
		return;
	}
	_enqueue_raw_response(_response.get_raw(), is_file_response);

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

	std::string route = loc.route;
	if (route[route.size() - 1] == '/')
		route = route.substr(0, route.size() - 1);

	path = path.substr(route.size());

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
	_cgi = new CgiSession();

	if (!_cgi->start(_request, *_server_config, loc, script_path, interpreter, cwd))
	{
		_deliver_cgi_result();
		return;
	}

	_state = _cgi->stdin_done() ? CGI_READING_STDOUT : CGI_WRITING_STDIN;
	LOG_CLIENT_D() << "CgiSession started for fd=" << _fd
				   << " script=" << script_path;
}

int		Client::get_cgi_stdin_fd()	const
{
	return _cgi->get_stdin_fd();
}

int		Client::get_cgi_stdout_fd()	const
{
	return _cgi->get_stdout_fd();
}

void	Client::handle_cgi_stdin_writable()
{
	if(!_cgi)
		return;

	_cgi->on_stdin_writable();
	if (_cgi->stdin_done())
		_state = CGI_READING_STDOUT;
}
void	Client::handle_cgi_stdout_readable()
{
	if(!_cgi)
		return;

    _cgi->on_stdout_readable();
    if (_cgi->is_complete())
        _deliver_cgi_result();
}

void	Client::check_cgi_timeout()
{
	if (!_cgi)
		return;
	_cgi->check_timeout();
	if (_cgi->is_complete())
		_deliver_cgi_result();

}

void Client::_deliver_cgi_result()
{
    if (!_cgi)
		return;
    const CgiResult r = _cgi->get_result();
    _enqueue_raw_response(r.raw_response);
    _state = WRITING;
    delete _cgi;
    _cgi = NULL;
}

void	Client::_enqueue_raw_response(const std::string& raw, bool is_file)
{
	PendingResponse* pr = new PendingResponse();
	pr->write_buf		= raw;
	pr->write_offset	= 0;
	pr->is_file			= is_file;
	pr->write_stage		= WRITE_HEADER;
	if (is_file)
	{
		pr->file_path		= _response.get_file_path();
		pr->file_remaining	= _response.get_file_size();
	}
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
	LOG_CLIENT_I() << "Send File: " << pr->file_path << " complete!";
	_file_stream.close();
	return true;
}


void	Client::_handle_upload(const LocationConfig& loc)
{
	const	std::map<std::string, std::string>& headers = _request.get_headers();
	std::map<std::string, std::string>::const_iterator ct_it = headers.find("content-type");

	if (ct_it == headers.end())
	{
		bool is_file = _response.build_error(*_server_config, 415);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		LOG_CLIENT_W() << "POST: missing content-type in header";
		return;
	}

	std::string boundary = MultipartParser::extract_boundary(ct_it->second);
	if (boundary.empty())
	{
		bool is_file = _response.build_error(*_server_config, 400);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		LOG_CLIENT_W() << "Upload: missing boundary in Content-Type";
		return;
	}

	MultipartParser parser;
	if (!parser.parse(_request.get_body(), boundary))
	{
		bool is_file = _response.build_error(*_server_config, 400);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		LOG_CLIENT_W() << "Upload: multipart parse failed";
		return;
	}

	const std::vector<MultipartPart>& parts = parser.get_parts();
	if (parts.empty())
	{
		bool is_file = _response.build_error(*_server_config, 400);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		LOG_CLIENT_W() << "Upload: no parts found";
		return;
	}

	std::string saved_filename;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (parts[i].filename.empty())
			continue;
		const std::string& fn = parts[i].filename;
		if (fn.find('/') != std::string::npos || fn.find("..") != std::string::npos)
		{
			bool is_file = _response.build_error(*_server_config, 400);
			_enqueue_raw_response(_response.get_raw(), is_file);
			_state = WRITING;
			LOG_CLIENT_W() << "Upload: dangerous filename rejected: " << fn;
			return;
		}

		std::string dest = loc.upload_store + "/" + fn;
		LOG_CLIENT_I() << "Upload: saving to " << dest;

		std::ofstream out(dest.c_str(), std::ios::binary);
		if (!out.is_open())
		{
			bool is_file = _response.build_error(*_server_config, 500);
			_enqueue_raw_response(_response.get_raw(), is_file);
			_state = WRITING;
			LOG_CLIENT_W() << "Upload: cannot open dest file: " << dest;
			return;
		}
		out.write(parts[i].body.c_str(), static_cast<std::streamsize>(parts[i].body.size()));
		out.close();

		saved_filename = fn;
		LOG_CLIENT_I() << "Upload: saved " << fn << " (" << parts[i].body.size() << " bytes)";
		break;
	}

	if (saved_filename.empty())
	{
		bool is_file = _response.build_error(*_server_config, 400);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		return;
	}

	_response.build_upload_ok(saved_filename);
	_enqueue_raw_response(_response.get_raw());
	_state = WRITING;
}

void	Client::_handle_delete(const LocationConfig& loc)
{
	std::string	path = _request.get_uri();
	size_t		q = path.find('?');
	if (q != std::string::npos)
		path = path.substr(0, q);

	if (path.find("/../") != std::string::npos ||
		(path.size() >= 3 && path.substr(path.size() - 3) == "/.."))
	{
		bool is_file = _response.build_error(*_server_config, 403);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		LOG_CLIENT_W() << "DELETE: Try to acess /.. : 403";
		return;
	}

	std::string fs_path = loc.root + path;
	struct stat st;
	if (stat(fs_path.c_str(), &st) != 0)
	{
		bool is_file = _response.build_error(*_server_config, 404);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		LOG_CLIENT_W() << "DELETE: file doesn't exist : 404: " << fs_path;
		return;
	}

	if (S_ISDIR(st.st_mode))
	{
		bool is_file = _response.build_error(*_server_config, 403);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		LOG_CLIENT_W() << "DELETE: Unable to delte a directory";
		return;
	}

	if (unlink(fs_path.c_str()) != 0)
	{
		bool is_file = _response.build_error(*_server_config, 403);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
        LOG_CLIENT_E() << "DELETE: unlink failed on " << fs_path << ": " << strerror(errno);
		return;
	}

	_response.build_delete_ok();
	_enqueue_raw_response(_response.get_raw());
	_state = WRITING;
}

void	Client::_handle_autoindex(const std::string& uri, const LocationConfig& loc)
{
	(void)uri;
	std::string	path = _request.get_uri();
	size_t		q = path.find('?');
	if (q != std::string::npos)
		path = path.substr(0, q);

	if (path.empty() || path[path.size() - 1] != '/')
	{
		std::string raw =
			"HTTP/1.1 301 Moved Permanently\r\n"
			"Location: " + path + "/\r\n"
			"Content-Length: 0\r\n"
			"Connection: close\r\n"
			"\r\n";
		_enqueue_raw_response(raw);
		_state = WRITING;
		return;
	}

	std::string fs_path = loc.root + path;
	std::string html = Autoindex::generate(path, fs_path);
	if (html.empty())
	{
		bool is_file = _response.build_error(*_server_config, 403);
		_enqueue_raw_response(_response.get_raw(), is_file);
		_state = WRITING;
		return;
	}
	_response.build_autoindex(path, html);
	_enqueue_raw_response(_response.get_raw());
	_state = WRITING;
}
