#pragma once

#include "CgiHandler.hpp"
#include "Request.hpp"
#include "Config.hpp"

#include <string>
#include <map>

struct CgiResult
{
	std::string	raw_response;
	bool		is_error;

	CgiResult() : is_error(false) {}
};

class CgiSession
{
public:
	CgiSession();
	~CgiSession();

	bool	start(	const Request&			req,
					const ServerConfig&		server,
					const LocationConfig&	loc,
					const std::string&		script_path,
					const std::string&		interpreter,
					const std::string&		cwd);
	void	on_stdin_writable();
	void	on_stdout_readable();
	void	check_timeout();

  	bool 				stdin_done()	const { return _handler.stdin_done(); }
    bool    			is_complete()	const { return _complete; }
	const CgiResult&	get_result()	const { return _result; }
	bool				is_running()	const { return _handler.get_state() == CGI_RUNNING; }

	int		get_stdin_fd()	const;
	int		get_stdout_fd()	const;

private:
    CgiSession(const CgiSession&);
    CgiSession& operator=(const CgiSession&);

	CgiResult			_build_result()	const;
	static CgiResult	_build_error_result(int code, const std::string& phrase);

	CgiHandler	_handler;
	bool		_complete;
	CgiResult	_result;
};
