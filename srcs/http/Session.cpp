#include "Session.hpp"
#include "Logger.hpp"
#include "Utilizer.hpp"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────
// SessionStore
// ─────────────────────────────────────────────

SessionStore& SessionStore::instance()
{
	static	SessionStore	inst;
	return inst;
}

SessionStore::SessionStore()
{
	std::srand(static_cast<unsigned int>(time(NULL)));
}

SessionStore::~SessionStore() {}

std::string	SessionStore::_generate_id()
{
	const char hex[] = "0123456789abcdef";
	std::string	id;
	id.reserve(32);
	for (int i = 0; i < 32; ++i)
		id += hex[std::rand() % 16];
	return id;
}

SessionData*	SessionStore::create()
{
	std::string	new_id;
	do
	{
		new_id = _generate_id();
	} while (_sessions.count(new_id));

	SessionData& sd = _sessions[new_id];
	sd.session_id	= new_id;
	sd.created_at	= time(NULL);
	sd.last_access	= time(NULL);

	LOG_CLIENT_I() << "New session created: " << new_id;
	return	&sd;
}

SessionData*	SessionStore::search(const std::string& cookie_header)
{
	if (cookie_header.empty())
		return NULL;

	std::string sid = CookieParser::get(cookie_header, "session_id");
	if (sid.empty())
		return NULL;

	std::map<std::string, SessionData>::iterator it = _sessions.find(sid);
	if (it == _sessions.end())
		return NULL;

	if (time(NULL) - it->second.last_access > SESSION_LIFETIME)
	{
        LOG_CLIENT_D() << "Session expired: " << sid;
		_sessions.erase(it);
		return NULL;
	}

	it->second.last_access = time(NULL);
	return &it->second;
}

void			SessionStore::destroy(const std::string& session_id)
{
	_sessions.erase(session_id);
    LOG_CLIENT_I() << "Session destroyed: " << session_id;
}

void			SessionStore::purge_expired()
{
	time_t	now = time(NULL);
	std::map<std::string, SessionData>::iterator it = _sessions.begin();
	while (it != _sessions.end())
	{
		if (now - it->second.last_access > SESSION_LIFETIME)
		{
            LOG_CLIENT_D() << "Purging expired session: " << it->second.session_id;
			std::map<std::string, SessionData>::iterator to_erase = it;
			++it;
			_sessions.erase(to_erase);
		}
		else
			++it;
	}
}

// ─────────────────────────────────────────────
// CookieParser
// ─────────────────────────────────────────────

std::map<std::string, std::string>	CookieParser::parse(const std::string& cookie_header)
{
	std::map<std::string, std::string> res;
	if (cookie_header.empty())
		return res;

	std::string	s = cookie_header;
	size_t		pos = 0;

	while (pos < s.size())
	{
		size_t	semi = s.find(';', pos);
		std::string	pair;
		if (semi == std::string::npos)
		{
			pair = s.substr(pos);
			pos	= s.size();
		}
		else
		{
			pair = s.substr(pos, semi - pos);
			pos = semi + 1;
		}

		pair = Utilizer::trim(pair);

		size_t	eq = pair.find('=');
		if (eq == std::string::npos)
			continue;

		std::string key = pair.substr(0, eq);
		std::string val = pair.substr(eq + 1);

		key = Utilizer::trim(key);
		val = Utilizer::trim(val);

		if (!key.empty())
			res[key] = val;
	}
	return res;
}

std::string	CookieParser::get(const std::string& cookie_header, const std::string& key)
{
	std::map<std::string, std::string> cookies = parse(cookie_header);
	std::map<std::string, std::string>::const_iterator c_it = cookies.find(key);

	if (c_it == cookies.end())
		return "";
	return c_it->second;
}
