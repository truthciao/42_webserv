#pragma once

#include <string>
#include <map>

enum RequestParseState
{
	PARSE_REQUEST_LINE,
	PARSE_HEADERS,
	PARSE_BODY,
	PARSE_CHUNK_SIZE,
	PARSE_CHUNK_DATA,
	PARSE_CHUNK_CRLF,
	PARSE_CHUNK_TRAILER,
	PARSE_COMPLETE,
	PARSE_ERROR
};

struct ServerConfig;
class Request
{
public:
	Request();
	~Request();

	bool		feed(const char* data, size_t len, const ServerConfig& server);
	std::string	take_leftover();

	bool	is_complete()		const { return _parse_state == PARSE_COMPLETE; }
	bool	has_error()			const { return _parse_state == PARSE_ERROR; }
	bool	is_body_too_large()	const { return _body_too_large; }

	const std::string&							get_method()	const { return _method; }
	const std::string&							get_uri()		const { return _uri; }
	const std::string&							get_version()	const { return _version; }
	const std::map<std::string, std::string>&	get_headers()	const { return _headers; }
	const std::string&							get_body()		const { return _body; }
	// size_t										get_max_body_size() const	{ return _max_body_size; }

	void	print() const;
	void 	reset();

private:
	std::string			_raw_buf;
	RequestParseState	_parse_state;

	std::string							_method;
	std::string							_uri;
	std::string							_version;
	std::map<std::string, std::string>	_headers;
	std::string							_body;

	size_t	_content_len;
	bool	_is_chunked;
	size_t	_chunk_size;
	size_t	_max_body_size;
	bool	_body_too_large;

	bool	parse_request_line(const std::string& line);
	bool	parse_header_line(const std::string& line);
	bool	parse_body();
	bool	parse_chunked_body();

	std::string trim(const std::string& s) const;
	bool		hex_string_to_size(const std::string& s, size_t& out) const;
};
