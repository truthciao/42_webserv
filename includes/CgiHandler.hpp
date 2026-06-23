#pragma once

#include <string>
#include <vector>
#include <map>
#include <sys/types.h>

#define CGI_TIMEOUT_SECONDS	5

enum CgiState
{
	CGI_IDLE,
	CGI_RUNNING,
	CGI_DONE,
	CGI_ERROR,
	CGI_TIMEOUT
};

class CgiHandler
{
public:
	CgiHandler();
	~CgiHandler();

	bool	start(const std::string& script_path,
				  const std::string& interpreter,
				  const std::map<std::string, std::string>& env_vars,
				  const std::string& request_body,
				  const std::string& cwd);

	bool	write_to_stdin();
	bool	read_from_stdout();

	void	check_child_status();
	void	kill_child();

	int		get_stdin_fd()	const	{ return _stdin_fd[1]; }
	int		get_stdout_fd()	const	{ return _stdout_fd[0]; }

	bool	stdin_done()	const	{ return _stdin_done; }
	bool	stdout_done()	const	{ return _stdout_done; }

	CgiState			get_state()			const	{ return _state; }
	const std::string&	get_output()		const	{ return _output; }
	time_t				get_start_time()	const	{ return _start_time; }

	void	close_stdin_fd();
	void	close_stdout_fd();

private:
	CgiHandler(const CgiHandler&);
	CgiHandler& operator=(const CgiHandler&);

	pid_t	_pid;

	int		_stdin_fd[2];
	int		_stdout_fd[2];

	std::string	_request_body;
	size_t		_body_offset;
	bool		_stdin_done;

	std::string	_output;
	bool		_stdout_done;

	CgiState	_state;
	time_t		_start_time;

	static char**	build_argv(const std::string& interpreter, const std::string& script_path);
	static char**	build_envp(const std::map<std::string, std::string>& env_vars);
	static void		free_charpp(char** arr);
};
