#include "CgiEnvBuilder.hpp"

#include <sstream>
#include <algorithm>
#include <cctype>


std::map<std::string, std::string>
CgiEnvBuilder::build(	const Request&			req,
						const ServerConfig&		server,
						const LocationConfig& 	loc,
						const std::string& 		script_path)
{
	std::map<std::string, std::string> env;

	const std::map<std::string, std::string>& headers = req.get_headers();

    // ── Standard CGI/1.1 meta-variables ──────────────────────────────────
	env["GATEWAY_INTERFACE"] = "CGI/1.1";
	env["SERVER_PROTOCOL"]   = req.get_version();
	env["SERVER_SOFTWARE"]   = "webserv/1.0";
	env["REQUEST_METHOD"]    = req.get_method();

    // ── Server info ───────────────────────────────────────────────────────
	{
		std::ostringstream	port_oss;
		port_oss << server.port;
		env["SERVER_PORT"]	= port_oss.str();
	}
	env["SERVER_NAME"] = server.server_names.empty()
						 ? "localhost"
						 : server.server_names[0];
	env["DOCUMENT_ROOT"] = loc.root;

    // ── URI decomposition ─────────────────────────────────────────────────
	std::string	raw_uri = req.get_uri();
	std::string	path_info;
	std::string	query_str;

	size_t	q = raw_uri.find('?');
	if (q != std::string::npos)
	{
		path_info	= raw_uri.substr(0, q);
		query_str	= raw_uri.substr(q + 1);
	}
	else
		path_info = raw_uri;

	env["PATH_INFO"]       = path_info;
	env["QUERY_STRING"]    = query_str;
	env["SCRIPT_NAME"]     = path_info;
	env["SCRIPT_FILENAME"] = script_path;

    // ── Body metadata ─────────────────────────────────────────────────────
	std::map<std::string, std::string>::const_iterator it;

	{
		std::ostringstream cl;
		cl << req.get_body().size();
		env["CONTENT_LENGTH"] = cl.str();
	}

	it = headers.find("content-type");
	if (it != headers.end())
		env["CONTENT_TYPE"] = it->second;

    // ── HTTP_* headers ────────────────────────────────────────────────────
	for (it = headers.begin(); it != headers.end(); ++it)
	{
		std::string key = "HTTP_";
		for (size_t i = 0; i < it->first.size(); ++i)
		{
			char c = it->first[i];
			key += (c == '-') ? '_' : static_cast<char>(std::toupper(c));
		}
		if (key == "HTTP_CONTENT_LENGTH" || key == "HTTP_CONTENT_TYPE")
			continue;
		env[key] = it->second;
	}

    // php-cgi security requirement
	env["REDIRECT_STATUS"] = "200";

	return env;
}
