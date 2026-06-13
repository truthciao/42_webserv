#pragma once

#include <string>
#include <map>

class Response
{
public:
	Response();

	void	build(const std::string& uri, const std::string& root);

	const std::string& get_raw() const { return _raw; }

private:
	int			_status_code;
	std::string	_status_text;
	std::string	_body;
	std::string	_raw;

	static std::string	get_mime_type(const std::string& path);
	static bool			read_file(const std::string& path, std::string& body);
	static bool			file_exists(const std::string& path);
	static bool			is_directory(const std::string& path);
	static std::string	status_text(int code);

	void	build_error(int code);
	void	serialize(const std::string& content_type);

	static std::map<std::string, std::string>	_mime_types;
	static std::map<std::string, std::string>	init_mime_types();
};
