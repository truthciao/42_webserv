#pragma	once

#include <string>
#include <map>
#include <ctime>

struct SessionData
{
	std::string	session_id;
	time_t		created_at;
	time_t		last_access;

	std::map<std::string, std::string>	data;

	SessionData()
		: created_at(time(NULL))
		, last_access(time(NULL))
	{}
};

class SessionStore
{
public:
	static SessionStore& instance();

	SessionData*	create();
	SessionData*	search(const std::string& cookie_header);
	void			destroy(const std::string& session_id);
	void			purge_expired();

	static const time_t	SESSION_LIFETIME = 1800;

private:
	SessionStore();
	~SessionStore();
	SessionStore(const SessionStore&);
	SessionStore& operator=(const SessionStore&);

	static	std::string	_generate_id();

	std::map<std::string, SessionData>	_sessions;
};

class CookieParser
{
public:
	static std::map<std::string, std::string>	parse(const std::string& cookie_header);

	static std::string	get(const std::string& cookie_header, const std::string& key);

private:
	CookieParser();
	~CookieParser();
};


