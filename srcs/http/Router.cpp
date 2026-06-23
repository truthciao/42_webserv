#include "Router.hpp"
#include "Logger.hpp"

#include <vector>

bool Router::is_prefix(const std::string& prefix, const std::string& uri)
{
	std::string pre;

	if (prefix[prefix.size() - 1] == '/')
		pre = prefix.substr(0, prefix.size() - 1);
	else
		pre = prefix;

	if (uri.size() < pre.size())
		return false;
	return uri.compare(0, pre.size(), pre) == 0;
}

bool Router::match(const ServerConfig& server, const std::string& uri, LocationConfig& out)
{
	bool	found = false;
	size_t	best_len = 0;

	for (std::vector<LocationConfig>::const_iterator it = server.locations.begin();
		 it != server.locations.end(); ++it)
	{
		if (!is_prefix(it->route, uri))
			continue;

		if (!found || it->route.size() > best_len)
		{
			out = *it;
			best_len = it->route.size();
			found = true;
		}
	}

	if (found)
		LOG_CONFIG_D() << "Router matched uri=" << uri << " -> location=" << out.route;
	else
		LOG_CONFIG_W() << "Router found no matching location for uri=" << uri;

	return found;
}
