#include "Request.hpp"
#include "Config.hpp"
#include "Router.hpp"
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
	, _is_chunked(false)
	, _chunk_size(0)
	, _max_body_size(static_cast<size_t>(-1))
	, _body_too_large(false)
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
	_is_chunked = false;
	_chunk_size = 0;
	_max_body_size = static_cast<size_t>(-1);
	_body_too_large = false;
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

bool	Request::feed(const char* data, size_t len, const ServerConfig& server)
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
					LocationConfig loc;
					if (Router::match(server, _uri, loc))
						_max_body_size = loc.client_max_body_size;
					else
						_max_body_size = server.client_max_body_size;

					std::map<std::string, std::string>::const_iterator te_it = _headers.find("transfer-encoding");
					std::map<std::string, std::string>::const_iterator cl_it = _headers.find("content-length");

					if (te_it != _headers.end())
					{
						std::string	te = te_it->second;
						std::transform(te.begin(), te.end(), te.begin(), ::tolower);
						if (te.find("chunked") != std::string::npos)
							_is_chunked = true;
					}

					if (!_is_chunked && cl_it != _headers.end())
					{
						std::istringstream iss(cl_it->second);
						iss >> _content_len;
					}

					if (_is_chunked)
						_parse_state = PARSE_CHUNK_SIZE;
					else if (_content_len > 0)
					{
						if (_content_len > _max_body_size)
						{
							LOG_REQUEST_W() << "Content-Length " << _content_len
											<< " exceeds max_body_size " << _max_body_size << ", rejecting";
							_body_too_large = true;
							_parse_state = PARSE_ERROR;
							return false;
						}
						_parse_state = PARSE_BODY;
					}
					else
					{
						_parse_state = PARSE_COMPLETE;
						return true;
					}
				}
				else if (!parse_header_line(line))
				{
					_parse_state = PARSE_ERROR;
					return false;
				}
			}
		}
		else if (_parse_state == PARSE_BODY)
		{

			if (!parse_body())
				break;
			return true;
		}
		else if (  _parse_state == PARSE_CHUNK_SIZE
				|| _parse_state == PARSE_CHUNK_DATA
				|| _parse_state == PARSE_CHUNK_CRLF
				|| _parse_state == PARSE_CHUNK_TRAILER )
		{
			if (!parse_chunked_body())
				break;
			if (_parse_state == PARSE_COMPLETE)
				return true;
		}
		else
			break;
	}

	return (_parse_state == PARSE_COMPLETE);
}

// ─────────────────────────────────────────────
// Content-Length body
// ─────────────────────────────────────────────

bool	Request::parse_body()
{
	if (_raw_buf.size() < _content_len)
		return false;

	_body = _raw_buf.substr(0, _content_len);
	_raw_buf.erase(0, _content_len);

	_parse_state = PARSE_COMPLETE;
	return true;
}

// ─────────────────────────────────────────────
// Chunked body
//   chunk format: <hex-size>\r\n<data>\r\n ... 0\r\n\r\n
// ─────────────────────────────────────────────

bool	Request::hex_string_to_size(const std::string& s, size_t& out) const
{
	if (s.empty())
		return false;

	std::istringstream iss(s);
	iss >> std::hex >> out;

	return !iss.fail();
}

bool	Request::parse_chunked_body()
{
	while (true)
	{
		if (_parse_state == PARSE_CHUNK_SIZE)
		{
			size_t crlf_pos = _raw_buf.find("\r\n");
			if (crlf_pos == std::string::npos)
				return false;

			std::string size_line = _raw_buf.substr(0, crlf_pos);
			_raw_buf.erase(0, crlf_pos + 2);

			//"size;ext=val"
			size_t semi = size_line.find(';');
			if (semi != std::string::npos)
				size_line = size_line.substr(0, semi);
			size_line = trim(size_line);

			size_t chunk_size = 0;
			if (!hex_string_to_size(size_line, chunk_size))
			{
				LOG_RESPONSE_E() << "Invalid chunk size: \"" << size_line << "\"";
				_parse_state = PARSE_ERROR;
				return false;
			}

			_chunk_size = chunk_size;

			if (_chunk_size == 0)
				_parse_state = PARSE_CHUNK_TRAILER;
			else
				_parse_state = PARSE_CHUNK_DATA;
		}
		else if (_parse_state == PARSE_CHUNK_DATA)
		{
			if (_raw_buf.size() < _chunk_size)
				return false;

			if (_body.size() + _chunk_size > _max_body_size)
			{
				LOG_CGI_W() << "Chunked body size " << (_body.size() + _chunk_size)
							<< " exceeds max_body_size" << _max_body_size;
				_body_too_large = true;
				_parse_state = PARSE_ERROR;
				return false;
			}

			_body.append(_raw_buf, 0, _chunk_size);
			_raw_buf.erase(0, _chunk_size);

			_parse_state = PARSE_CHUNK_CRLF;
		}
		else if (_parse_state == PARSE_CHUNK_CRLF)
		{
			if (_raw_buf.size() < 2)
				return false;

			if (_raw_buf[0] != '\r' || _raw_buf[1] != '\n')
			{
				LOG_REQUEST_E() << "Malformed chunk terminator";
				_parse_state = PARSE_ERROR;
				return false;
			}
			_raw_buf.erase(0, 2);

			_parse_state = PARSE_CHUNK_SIZE;
		}
		else if (_parse_state == PARSE_CHUNK_TRAILER)
		{
			size_t crlf_pos = _raw_buf.find("\r\n");
			if (crlf_pos == std::string::npos)
				return false;

			std::string line = _raw_buf.substr(0, crlf_pos);
			_raw_buf.erase(0, crlf_pos + 2);

			_parse_state = PARSE_COMPLETE;
			return true;
		}
		else
			break;
	}
	return false;
}

bool	Request::parse_request_line(const std::string& line)
{
	std::istringstream iss(line);

	if (!(iss >> _method >> _uri >> _version))
	{
		LOG_REQUEST_E() << "Malformed request line: \"" << line << "\"";
		return false;
	}

	if (_version != "HTTP/1.0" && _version != "HTTP/1.1")
	{
		LOG_REQUEST_E() << "Unsupported HTTP version: " << _version;
		return false;
	}

	LOG_REQUEST_I() << "Requesting: " << _uri << ", method: " << _method;
	return true;
}

bool	Request::parse_header_line(const std::string& line)
{
	size_t	colon_pos = line.find(':');
	if (colon_pos == std::string::npos)
	{
		LOG_REQUEST_E() << "Malformed header line: \"" << line << "\"";
		return false;
	}

	std::string	key		= line.substr(0, colon_pos);
	std::string	value	= line.substr(colon_pos + 1);

	std::transform(key.begin(), key.end(), key.begin(), ::tolower);
	key		= trim(key);
	value	= trim(value);

	if (key.empty())
	{
		LOG_REQUEST_E() << "Empty header key";
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
	LOG_REQUEST_D() << "=== Parsed Request ===";
	LOG_REQUEST_D() << "Method  : " << _method ;
	LOG_REQUEST_D() << "URI     : " << _uri    ;
	LOG_REQUEST_D() << "Version : " << _version;
	LOG_REQUEST_D() << "Headers :";

	for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
		 it != _headers.end(); ++it)
	{
		LOG_REQUEST_D() << "  [" << it->first << "] = " << it->second;
	}

	if (!_body.empty())
    {
        const size_t max_body_log_len = 500;
        if (_body.length() <= max_body_log_len)
        {
            LOG_REQUEST_D() << "Body    : " << _body;
        }
        else
        {
            LOG_REQUEST_D() << "Body    : " << _body.substr(0, max_body_log_len) << "... [truncated, total size: " << _body.length() << " bytes]";
        }
    }
    else
    {
        LOG_REQUEST_D() << "Body    : [empty]";
    }

	LOG_REQUEST_D() << "======================";
}


