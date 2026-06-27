#pragma once

#include "Config.hpp"

#include <string>
#include <map>

class Response
{
public:
	Response();

	bool	build(	const std::string& method,
					const std::string& uri,
					const ServerConfig& server,
					const LocationConfig& location);
	bool	build_no_location(const ServerConfig& _server_cfg);
	bool	build_error(const ServerConfig& server, int code);
	void	build_upload_ok(const std::string& filename);
	void	build_delete_ok();
	void	build_autoindex(const std::string& uri, const std::string& html_body);

	static void	inject_header(std::string& raw, const std::string& key, const std::string&);

	const std::string&	get_raw() 		const { return _raw; }
	const std::string&	get_file_path() const { return _file_path; }
	size_t				get_file_size() const { return _file_size; }
	bool				is_autoindex_needed()	const { return _autoindex_needed; }

private:
	int			_status_code;
	std::string	_status_text;
	std::string	_body;
	std::string	_raw;
	std::string	_file_path;
	size_t		_file_size;
	bool		_autoindex_needed;

	static std::string	get_mime_type(const std::string& path);
	static bool			file_exists(const std::string& path);
	static bool			is_directory(const std::string& path);
	static std::string	status_text(int code);
	static bool			get_file_size_byptes(const std::string& path, size_t& out_size);

	bool	try_server_custom_error_page(const ServerConfig& server, int code);
	void	build_error(int code);
	void	build_redirect(int code, const std::string& location_url);
	void	build_file_header(const std::string& content_type);

	static std::map<std::string, std::string>	_mime_types;
	static std::map<std::string, std::string>	init_mime_types();
};
