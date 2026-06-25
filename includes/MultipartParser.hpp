#pragma once

#include <string>
#include <vector>
#include <map>

struct MultipartPart
{
	std::map<std::string, std::string>	headers;
	std::string							filename;
	std::string							name;
	std::string							body;
};

class MultipartParser
{
public:
	MultipartParser();
	~MultipartParser();

	static std::string	extract_boundary(const std::string& content_type);

	bool				parse(const std::string& body, const std::string& boundary);
	static void			parse_disposition(const std::string& value,
										  std::string& out_name,
										  std::string& out_filename);
	const std::vector<MultipartPart>& get_parts() const { return _parts; }

private:
	MultipartParser(const MultipartParser&);
	MultipartParser& operator=(const MultipartParser&);

	bool				parse_part(const std::string& raw_part);

	static std::string	trim(const std::string& s);


	std::vector<MultipartPart>	_parts;
};
