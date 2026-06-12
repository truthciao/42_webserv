#include "Request.hpp"
#include "Logger.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────

Request::Request()
	: _parse_state(PARSE_REQUEST_LINE)
	, _content_len(0)
{}

Request::~Request() {}

void 	Request::reset()
{
	_raw_buf.clear();
	_method.clear();
	_uri.clear();
	_version.clear();
	_headers.clear();
	_body.clear();
	_parse_state = PARSE_REQUEST_LINE;
	_content_len = 0;
}

// ─────────────────────────────────────────────
// Main entry: feed raw bytes in
// ─────────────────────────────────────────────

std::string Request::take_leftover()
{
	std::string leftover = _raw_buf;
	_raw_buf.clear();
	return leftover;
}

bool	Request::feed(const char* data, size_t len)
{
	_raw_buf.append(data, len);

	while (true)
	{
		if (_parse_state == PARSE_REQUEST_LINE || _parse_state == PARSE_HEADERS)
		{
			size_t	crlf_pos = _raw_buf.find("\r\n");
			if (crlf_pos == std::string::npos)
				break;

			std::string	line = _raw_buf.substr(0, crlf_pos);
			_raw_buf.erase(0, crlf_pos + 2);

			if (_parse_state == PARSE_REQUEST_LINE)
			{
				if (!parse_request_line(line))
				{
					_parse_state = PARSE_ERROR;
					return false;
				}
				_parse_state = PARSE_HEADERS;
			}
			else if (_parse_state == PARSE_HEADERS)
			{
				if (line.empty())
				{
					std::map<std::string, std::string>::const_iterator it = _headers.find("content-length");

					if (it != _headers.end())
					{
						std::istringstream iss(it->second);
						iss >> _content_len;
					}

					if (_content_len > 0)
						_parse_state = PARSE_BODY;
					else
					{
						_parse_state = PARSE_COMPLETE;
						return true;
					}
				}
				if (!parse_header_line(line))
				{
					_parse_state = PARSE_ERROR;
					return false;
				}
			}
		}
		else if (_parse_state == PARSE_BODY)
		{
			if (_raw_buf.size() < _content_len)
				break;

			_body = _raw_buf.substr(0, _content_len);
			_raw_buf.erase(0, _content_len);

			_parse_state = PARSE_BODY;
			return true;
		}
		else
			break;
	}

	return (_parse_state == PARSE_COMPLETE);
}

bool	Request::parse_request_line(const std::string& line)
{
	std::istringstream iss(line);

	if (!(iss >> _method >> _uri >> _version))
	{
		LOG_REQUEST_E() << "[Request] Malformed request line: \"" << line << "\"";
		return false;
	}

	if (_version != "HTTP/1.0" && _version != "HTTP/1.1")
	{
		LOG_REQUEST_E() << "[Request] Unsupported HTTP version: " << _version;
		return false;
	}

	return true;
}
bool	Request::parse_header_line(const std::string& line)
{
	size_t	colon_pos = line.find(':');
	if (colon_pos == std::string::npos)
	{
		LOG_REQUEST_E() << "[Request] Malformed header line: \"" << line << "\"";
		return false;
	}

	std::string	key		= line.substr(0, colon_pos);
	std::string	value	= line.substr(colon_pos + 1);

	std::transform(key.begin(), key.end(), key.begin(), ::tolower);
	key		= trim(key);
	value	= trim(value);

	if (key.empty())
	{
		LOG_REQUEST_E() << "[Request] Empty header key";
		return false;
	}

	_headers[key] = value;
	return true;
}

// ─────────────────────────────────────────────
// Utility: trim and print
// ─────────────────────────────────────────────

std::string Request::trim(const std::string& s) const
{
	const std::string whitespace = " \t\r\n";

	size_t start = s.find_first_not_of(whitespace);
	if (start == std::string::npos)
		return "";

	size_t end = s.find_last_not_of(whitespace);
	return s.substr(start, end - start + 1);
}

void    Request::print() const
{
	LOG_REQUEST_I() << "=== Parsed Request ===";
	LOG_REQUEST_I() << "Method  : " << _method ;
	LOG_REQUEST_I() << "URI     : " << _uri    ;
	LOG_REQUEST_I() << "Version : " << _version;
	LOG_REQUEST_I() << "Headers :";

	for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
		 it != _headers.end(); ++it)
	{
		LOG_REQUEST_I() << "  [" << it->first << "] = " << it->second;
	}
	LOG_REQUEST_I() << "======================";
}


