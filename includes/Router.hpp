#pragma once

#include "Config.hpp"
#include <string>

class Router
{
public:
	static bool match(const ServerConfig& server, const std::string& uri, LocationConfig& out);

private:
	Router(/* args */);
	~Router();
	Router(const Router&);
	Router& operator=(const Router&);

	static bool is_prefix(const std::string& prefix, const std::string& uri);
};
