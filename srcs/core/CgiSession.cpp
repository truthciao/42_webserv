#include "CgiSession.hpp"
#include "CgiEnvBuilder.hpp"
#include "Logger.hpp"

#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

CgiSession::CgiSession() : _complete(false) {}
CgiSession::~CgiSession() {}

// ─────────────────────────────────────────────────────────────────────────────
// start()
// ─────────────────────────────────────────────────────────────────────────────

bool	CgiSession::start(	const Request&			req,
							const ServerConfig&		server,
							const LocationConfig&	loc,
							const ScriptInfo& script)
{
	std::map<std::string, std::string> env = CgiEnvBuilder::build(req, server, loc, script.script_path);

	if (!_handler.start(script.script_name, script.interpreter, env, req.get_body(), script.cwd))
	{
		LOG_CGI_E() << "CgiSession::start failed for script=" << script.script_path;
		_result		= _build_error_result(500, "Internal Server Error");
		_complete	= true;
		return	false;
	}

	LOG_CGI_I() << "CgiSession started: script=" << script.script_path;
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Poll-driven callbacks
// ─────────────────────────────────────────────────────────────────────────────

void	CgiSession::on_stdin_writable()
{
	if (_complete)
		return;
	_handler.write_to_stdin();
}

void	CgiSession::on_stdout_readable()
{
	if (_complete)
		return;

	bool more = _handler.read_from_stdout();
	if (!more)
	{
		_handler.check_child_status();

		CgiState st = _handler.get_state();
		if (st == CGI_DONE || st == CGI_RUNNING)
		{
			_result		= _build_result();
			_complete	= true;
			LOG_CGI_I() << "CgiSession complete, output size=" << _handler.get_output().size();
		}
		else
		{
			_result		= _build_error_result(502, "Bad Gateway");
			_complete	= true;
			LOG_CGI_E() << "CgiSession: stdout EOF but CGI state=" << _handler.get_state();
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Timeout check (called every Server tick while CGI is running)
// ─────────────────────────────────────────────────────────────────────────────

void	CgiSession::check_timeout()
{
	if ( _complete || _handler.get_state() != CGI_RUNNING)
		return;

	_handler.check_child_status();

	if (_handler.get_state() == CGI_TIMEOUT)
	{
		LOG_CGI_W() << "CgiSession: timeout";

		_result   	= _build_error_result(504, "Gateway Timeout");
		_complete	= true;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// State queries
// ─────────────────────────────────────────────────────────────────────────────

int		CgiSession::get_stdin_fd()	const
{
	if (!_handler.stdin_done())
		return _handler.get_stdin_fd();
	return -1;
}

int		CgiSession::get_stdout_fd()	const
{
	if (!_handler.stdout_done())
		return _handler.get_stdout_fd();
	return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// _build_result(): parse CGI stdout → HTTP response
// ─────────────────────────────────────────────────────────────────────────────

CgiResult	CgiSession::_build_result()	const
{
	const std::string& raw_output = _handler.get_output();

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

	CgiResult	res;
	res.raw_response = h.str() + cgi_body;
	LOG_CGI_D() << h.str();
	res.is_error	 = false;
	return res;
}

CgiResult CgiSession::_build_error_result(int code, const std::string& phrase)
{
	std::ostringstream code_oss;
	code_oss << code;
	std::string body = "<html><body><h1>" + code_oss.str() + " " + phrase + "</h1></body></html>";

	std::ostringstream h;
	h << "HTTP/1.1 " << code_oss.str() << " " << phrase << "\r\n"
	  << "Content-Type: text/html\r\n"
	  << "Content-Length: " << body.size() << "\r\n"
	  << "Connection: close\r\n"
	  << "\r\n";

	CgiResult r;
	r.raw_response = h.str() + body;
	r.is_error     = true;
	return r;
}
