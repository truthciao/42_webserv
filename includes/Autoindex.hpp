#pragma once

#include <string>

class Autoindex
{
public:
	static std::string	generate(const std::string& uri_path,
								 const std::string& fs_path);

private:
	Autoindex();
	~Autoindex();
	Autoindex(const Autoindex&);
	Autoindex& operator=(const Autoindex&);

};
