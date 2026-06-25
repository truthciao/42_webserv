#pragma once

#include "Request.hpp"
#include "Config.hpp"

#include <map>
#include <string>

namespace	CgiEnvBuilder
{
	std::map<std::string, std::string>	build(	const Request&			req,
												const ServerConfig&		server,
												const LocationConfig& 	loc,
												const std::string& 		script_path);

	void printEnvMap(const std::map<std::string, std::string>& env_map);

}
