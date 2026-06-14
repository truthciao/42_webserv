#include "Response.hpp"
#include "Logger.hpp"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <iostream>

// ─────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────

Response::Response()
	: _status_code(200)
	, _status_text("OK")
	, _file_size(0)
{}

// ─────────────────────────────────────────────
//  Static member init
// ─────────────────────────────────────────────

std::map<std::string, std::string>	Response::_mime_types = Response::init_mime_types();

std::map<std::string, std::string>	Response::init_mime_types()
{
	std::map<std::string, std::string> m;
	m[".html"]  = "text/html";
	m[".htm"]   = "text/html";
	m[".css"]   = "text/css";
	m[".js"]    = "application/javascript";
	m[".json"]  = "application/json";
	m[".png"]   = "image/png";
	m[".jpg"]   = "image/jpeg";
	m[".jpeg"]  = "image/jpeg";
	m[".gif"]   = "image/gif";
	m[".svg"]   = "image/svg+xml";
	m[".ico"]   = "image/x-icon";
	m[".txt"]   = "text/plain";
	m[".pdf"]   = "application/pdf";
	return m;
}

// ─────────────────────────────────────────────
// Public interface
// ─────────────────────────────────────────────

bool	Response::build(const std::string& uri, const std::string& root)
{
	std::string		path = uri;
	const size_t	q = path.find('?');
	if (q != std::string::npos)
		path = path.substr(0, q);

	if (path.find("/../") != std::string::npos || (path.size() >= 3 && path.substr(path.size() - 3) == "/.."))
	{
		build_error(403);
		LOG_RESPONSE_D() << "Try to acess /.. : 403";
		return false;
	}

	std::string fs_path = root + path;

	if (is_directory(fs_path))
	{
		if (fs_path[fs_path.size() - 1] != '/')
		{
			_status_code = 301;
			_status_text = status_text(301);
			LOG_RESPONSE_D() << fs_path << " is a directory but requested without '/', redirecting...: 301";
			_raw =	"HTTP/1.1 301 Moved Permanently\r\n"
					"Location: " + uri + "/\r\n"
					"Content-Length: 0\r\n"
					"Connection: close\r\n"
					"\r\n";
			return false;
		}
		else
			fs_path += "index.html";
	}

	size_t	file_size = 0;
	if (file_exists(fs_path) && get_file_size_byptes(fs_path, file_size))
	{
		// std::string body;
		// if (!read_file(fs_path, body))
		// {
		// 	build_error(403);
		// 	LOG_RESPONSE_D() << fs_path << " open failed: 403";
		// 	return;
		// }
		_status_code = 200;
		_status_text = status_text(200);
		// _body = body;
		_file_path = fs_path;
		_file_size = file_size;
		// serialize(get_mime_type(fs_path));
		build_file_header(get_mime_type(fs_path));
		return true;
	}
	else
	{
		build_error(404);
		LOG_RESPONSE_D() << fs_path << " : file doesn't exit: 404";
		return false;
	}
}

std::string	Response::get_mime_type(const std::string& path)
{
	const	size_t	dot = path.rfind('.');
	if (dot == std::string::npos)
		return "application/octet-stream";

	const std::string ext = path.substr(dot);
	std::map<std::string, std::string>::const_iterator it = _mime_types.find(ext);
	if (it != _mime_types.end())
		return (it->second);

	return "application/octet-stream";
}

// bool	Response::read_file(const std::string& path, std::string& body)
// {
// 	std::ifstream file(path.c_str(), std::ios::binary);
// 	if (!file.is_open())
// 	{
// 		LOG_CGI_E() << "Error: open requested file failed!";
// 		return false;
// 	}

// 	std::ostringstream oss;
// 	oss << file.rdbuf();
// 	body = oss.str();

// 	return true;
// }

bool			Response::file_exists(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

bool			Response::is_directory(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

bool Response::get_file_size_byptes(const std::string& path, size_t& out_size)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return false;
	out_size = static_cast<size_t>(st.st_size);
	return true;
}


std::string	Response::status_text(int code)
{
	switch (code)
	{
		case 200: return "OK";
		case 301: return "Moved Permanently";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 500: return "Internal Server Error";
		default:  return "Unknown";
	}
}

void	Response::build_error(int code)
{
	_status_code = code;
	_status_text = status_text(code);

	std::ostringstream body;
	body	<< "<html><body>"
			<< "<h1>" << code << " " << _status_text << "</h1>"
			<< "</body></html>";
	_body = body.str();

	serialize("text/html");
}

void	Response::serialize(const std::string& content_type)
{
	std::ostringstream oss;
	oss << _body.size();

	std::string header;
	{
		std::ostringstream h;
		h	<< "HTTP/1.1 " << _status_code << " " << _status_text << "\r\n"
			<< "Content-Type: " << content_type << "\r\n"
			<< "Content-Length: " << _body.size() << "\r\n"
			<< "Connection: close\r\n"
			<< "\r\n";
		header = h.str();
	}
	_raw = header + _body;
}

void	Response::build_file_header( const std::string& content_type)
{
	std::ostringstream h;
	h	<< "HTTP/1.1 " << _status_code << " " << _status_text << "\r\n"
		<< "Content-Type: " << content_type << "\r\n"
		<< "Content-Length: " << _file_size << "\r\n"
		<< "Connection: close\r\n"
		<< "\r\n";
	_raw = h.str();
}
