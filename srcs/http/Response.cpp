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
	, _autoindex_needed(false)
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

bool	Response::build(	const std::string& method,
							const std::string& uri,
							const ServerConfig& server,
							const LocationConfig& location)
{
	if (!location.allow_methods.empty() &&
		location.allow_methods.find(method) == location.allow_methods.end())
	{
		build_error(server, 405);
		LOG_RESPONSE_D() << "Method " << method << " not allowed on " << location.route << ": 405";
		return false;
	}

	if (location.redirect_code != 0)
	{
		build_redirect(location.redirect_code, location.redirect_url);
		LOG_RESPONSE_D() << "Location " << location.route << " redirects to " << location.redirect_url;
		return false;
	}

	std::string		path = uri;
	const size_t	q = path.find('?');
	if (q != std::string::npos)
		path = path.substr(0, q);

	if (path.find("/../") != std::string::npos || (path.size() >= 3 && path.substr(path.size() - 3) == "/.."))
	{
		build_error(server, 403);
		LOG_RESPONSE_D() << "Try to acess /.. : 403";
		return true;
	}

	std::string route = location.route;
	if (route[route.size() - 1] == '/')
		route = route.substr(0, route.size() - 1);

	std::string fs_path = location.root + path.substr(route.size());

	if (is_directory(fs_path))
	{
		if (fs_path[fs_path.size() - 1] != '/')
		{
			_status_code = 301;
			_status_text = status_text(301);
			LOG_RESPONSE_I() << fs_path << " is a directory but requested without '/', redirecting...: 301";
			_raw =	"HTTP/1.1 301 Moved Permanently\r\n"
					"Location: " + uri + "/\r\n"
					"Content-Length: 0\r\n"
					"Connection: close\r\n"
					"\r\n";
			return false;
		}

		if (location.autoindex)
			_autoindex_needed = true;

		std:: string index_path = fs_path + (location.index.empty() ? "index.html" : location.index);
		if (file_exists(index_path))
			fs_path = index_path;
		else
		{
			_autoindex_needed = true;
			return false;
		}
	}

	size_t	file_size = 0;
	if (file_exists(fs_path) && get_file_size_byptes(fs_path, file_size))
	{
		_status_code = 200;
		_status_text = status_text(200);
		_file_path = fs_path;
		_file_size = file_size;
		build_file_header(get_mime_type(fs_path));
		return true;
	}
	else
	{
		build_error(server, 404);
		LOG_RESPONSE_W() << fs_path << " : file doesn't exit: 404";
		return true;
	}
}

bool	Response::build_no_location(const ServerConfig& _server_cfg)
{
	if (build_error(_server_cfg, 404))
	{
		LOG_RESPONSE_D() << "No location matched this uri: 404";
		return true;
	}
	return false;
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
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Request Entity Too Large";
		case 415: return "Unsupported Media Type";
		case 500: return "Internal Server Error";
		default:  return "Unknown";
	}
}

bool	Response::try_server_custom_error_page(const ServerConfig& server, int code)
{
	std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
	if (it == server.error_pages.end())
		return false;

	std::string fs_path = "." + it->second;
	size_t		file_size = 0;

	if (file_exists(fs_path) && get_file_size_byptes(fs_path, file_size))
	{
		_status_code = code;
		_status_text = status_text(code);
		_file_path = fs_path;
		_file_size = file_size;
		build_file_header(get_mime_type(fs_path));
		return true;
	}
	return false;
}

bool	Response::build_error(const ServerConfig& server, int code)
{
	if (try_server_custom_error_page(server, code))
	{
		LOG_RESPONSE_D() << "Served custom error page for code " << code;
		return true;
	}
	build_error(code);
	return false;
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

	std::string header;
	{
		std::ostringstream h;
		h	<< "HTTP/1.1 " << _status_code << " " << _status_text << "\r\n"
			<< "Content-Type: " << "text/html" << "\r\n"
			<< "Content-Length: " << _body.size() << "\r\n"
			<< "Connection: close\r\n"
			<< "\r\n";
		header = h.str();
	}
	_raw = header + _body;

	LOG_RESPONSE_D() << code << ": " << _status_text << ". Error response built successfully!";
}

void	Response::build_redirect(int code, const std::string& location_url)
{
	_status_code = code;
	_status_text = status_text(code);

	std::ostringstream h;
	h	<< "HTTP/1.1 " << _status_code << " " << _status_text << "\r\n"
		<< "Location: " << location_url << "\r\n"
		<< "Content-Length: 0\r\n"
		<< "Connection: close\r\n"
		<< "\r\n";
	_raw = h.str();
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

void	Response::build_upload_ok(const std::string& filename)
{
	_status_code = 201;
	_status_text = status_text(_status_code);

	std::ostringstream body;
	body << "<html><body>"
		 << "<h2>Upload successful</h2>"
		 << "<p>File <strong>" << filename << "</strong> has been uploaded.</p>"
		 << "</body></html>";
	_body = body.str();

	std::ostringstream h;
	h << "HTTP/1.1 " << _status_code << " " <<  _status_text << "\r\n"
	  << "Content-Type: text/html\r\n"
	  << "Content-Length: " << _body.size() << "\r\n"
	  << "Connection: close\r\n"
	  << "\r\n"
	  << _body;
	_raw = h.str();

	LOG_RESPONSE_I() << "201 Created: uploaded " << filename;
}
void	Response::build_delete_ok()
{
	_status_code = 204;
	_status_text = status_text(_status_code);

	std::ostringstream h;
	h << "HTTP/1.1 " << _status_code << " " <<  _status_text << "\r\n"
	  << "Content-Length: 0\r\n"
	  << "Connection: close\r\n"
	  << "\r\n";
	_raw = h.str();

	LOG_RESPONSE_I() << "204 No Content: resource deleted";
}

void	Response::build_autoindex(const std::string& uri, const std::string& html_body)
{
	(void)uri;
	_status_code = 200;
	_status_text = status_text(_status_code);
	_body		 = html_body;

	std::ostringstream h;
	h << "HTTP/1.1 " << _status_code << " " <<  _status_text << "\r\n"
	  << "Content-Type: text/html; charset=UTF-8\r\n"
	  << "Content-Length: " << _body.size() << "\r\n"
	  << "Connection: close\r\n"
	  << "\r\n"
	  << _body;
	_raw = h.str();

	LOG_RESPONSE_D() << "Autoindex page generated, size=" << _body.size();
}

void	Response::inject_header(std::string& raw, const std::string& key, const std::string& value)
{
	size_t	pos = raw.find("\r\n");
	if (pos == std::string::npos)
		return;

	std::string new_head_line = key + ": " + value + "\r\n";
	raw.insert(pos + 2, new_head_line);
}
