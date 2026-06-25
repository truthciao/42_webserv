#include "CgiHandler.hpp"
#include "Logger.hpp"

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define	READ_END	0
#define WRITE_END	1
#define CGI_CHUNK	4096

// ─────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────

CgiHandler::CgiHandler()
	: _pid(-1)
	, _body_offset(0)
	, _stdin_done(false)
	, _stdout_done(false)
	, _state(CGI_IDLE)
	, _start_time(0)
{
	_stdin_fd[0] = -1;
	_stdin_fd[1] = -1;
	_stdout_fd[0] = -1;
	_stdout_fd[1] = -1;
}

CgiHandler::~CgiHandler()
{
	close_stdin_fd();
	close_stdout_fd();

	if (_pid > 0 && _state == CGI_RUNNING)
	{
		kill(_pid, SIGKILL);
		int status;
		waitpid(_pid, &status, 0);
	}
}

void	CgiHandler::close_stdin_fd()
{
	if (_stdin_fd[0] >= 0)
	{
		close(_stdin_fd[0]);
		_stdin_fd[0] = -1;
	}
	if (_stdin_fd[1] >= 0)
	{
		close(_stdin_fd[1]);
		_stdin_fd[1] = -1;
	}
}

void	CgiHandler::close_stdout_fd()
{
	if (_stdout_fd[0] >= 0)
	{
		close(_stdout_fd[0]);
		_stdout_fd[0] = -1;
	}
	if (_stdout_fd[1] >= 0)
	{
		close(_stdout_fd[1]);
		_stdout_fd[1] = -1;
	}
}

// ─────────────────────────────────────────────
// argv / envp 构造
// ─────────────────────────────────────────────

char**	CgiHandler::build_argv(const std::string& interpreter,
							   const std::string& script_path)
{
	// argv[0] = interpreter (or script itself if interpreter empty)
	// argv[1] = script path
	// argv[2] = NULL
	char** argv = new char*[3];

	const std::string& arg0 = interpreter.empty() ? script_path : interpreter;

	argv[0] = new char[arg0.size() + 1];
	std::strcpy(argv[0], arg0.c_str());

	argv[1] = new char[script_path.size() + 1];
	std::strcpy(argv[1], script_path.c_str());

	argv[2] = NULL;
	return argv;
}
char**	CgiHandler::build_envp(const std::map<std::string, std::string>& env_vars)
{
	char **envp = new char*[env_vars.size() + 1];

	size_t i = 0;
	for (std::map<std::string, std::string>::const_iterator it = env_vars.begin();
		 it != env_vars.end(); ++it, ++i)
	{
		std::string entry = it->first + "=" + it->second;
		envp[i] = new char[entry.size() + 1];
		std::strcpy(envp[i], entry.c_str());
	}
	envp[i] = NULL;
	return envp;
}

void	CgiHandler::free_charpp(char** arr)
{
	if (!arr)
		return;
	for (size_t i = 0; arr[i] != NULL; ++i)
		delete[] arr[i];
	delete[] arr;
}

// ─────────────────────────────────────────────
// start(): fork + pipe + execve
// ─────────────────────────────────────────────

bool	CgiHandler::start(	const std::string& script_name,
							const std::string& interpreter,
							const std::map<std::string, std::string>& env_vars,
							const std::string& request_body,
							const std::string& cwd)
{
	if (pipe(_stdin_fd) == -1)
	{
		LOG_CGI_E() << "pipe(stdin) failed: " << strerror(errno);
		return false;
	}
	if (pipe(_stdout_fd) == -1)
	{
		LOG_CGI_E() << "pipe(stdout) failed: " << strerror(errno);
		close_stdin_fd();
		return false;
	}

	_request_body = request_body;
	_body_offset = 0;

	_stdin_done = _request_body.empty();

	_pid = fork();

	if (_pid < 0)
	{
		LOG_CGI_E() << "fork failed: " << strerror(errno);
		close_stdin_fd();
		close_stdout_fd();
		_state = CGI_ERROR;
		return false;
	}
	if (_pid == 0)
	{
		if (dup2(_stdin_fd[READ_END], STDIN_FILENO) == -1)
		{
			LOG_CGI_E() << "dup2(stdin) failed: " << strerror(errno);
			_exit(1);
		}
		if (dup2(_stdout_fd[WRITE_END], STDOUT_FILENO) == -1)
		{
			LOG_CGI_E() << "dup2(stdout) failed: " << strerror(errno);
			_exit(1);
		}

		close_stdin_fd();
		close_stdout_fd();

		if (!cwd.empty())
			chdir(cwd.c_str());

		char** argv = build_argv(interpreter, script_name);
		char** envp = build_envp(env_vars);

		LOG_CGI_I() << "Before execve: Scrip path =" << argv[1] << ", changed path to cwd=" << cwd;

		execve(argv[0], argv, envp);

		LOG_CGI_E() << "execve failed: " << strerror(errno);
		_exit(1);
	}

	close(_stdin_fd[READ_END]);
	close(_stdout_fd[WRITE_END]);
	_stdin_fd[READ_END] = -1;
	_stdout_fd[WRITE_END] = -1;

	if(fcntl(_stdin_fd[WRITE_END], F_SETFL, O_NONBLOCK) == -1)
	{
		LOG_CGI_E() << "fcntl O_NONBLOCK failed on stdin_fd=" << _stdin_fd[WRITE_END]
				  << ": " << strerror(errno);
		_state = CGI_ERROR;
		return false;
	}
	if(fcntl(_stdout_fd[READ_END], F_SETFL, O_NONBLOCK) == -1)
	{
		LOG_CGI_E() << "fcntl O_NONBLOCK failed on stdout_fd=" << _stdout_fd[READ_END]
				  << ": " << strerror(errno);
		_state = CGI_ERROR;
		return false;
	}

	_state = CGI_RUNNING;
	_start_time = time(NULL);

	LOG_CGI_I() << "CGI started: pid=" << _pid
				<< " script_name=" << script_name
				<< " stdin_fd=" << _stdin_fd[WRITE_END]
				<< " stdout_fd=" << _stdout_fd[READ_END];

	return true;
}

// ─────────────────────────────────────────────
// write_to_stdin()
// ─────────────────────────────────────────────

bool	CgiHandler::write_to_stdin()
{
	if (_stdin_done)
	{
		close_stdin_fd();
		return false;
	}

	size_t	remaining = _request_body.size() - _body_offset;
	size_t	to_write = (remaining < CGI_CHUNK) ? remaining : CGI_CHUNK;

	ssize_t	n = write(_stdin_fd[WRITE_END],
					  _request_body.c_str() + _body_offset,
					  to_write);

	if (n > 0)
	{
		_body_offset += n;
		if (_body_offset >= _request_body.size())
		{
			_stdin_done = true;
			close_stdin_fd();
		}
		return true;
	}
	else if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return true;
		LOG_CGI_E() << "Write error on stdin_fd: " << strerror(errno);
		_stdin_done = true;
		close_stdin_fd();
		return false;
	}

	LOG_CGI_D() << "Writing to stdin ended!";
	return true;
}

// ─────────────────────────────────────────────
// read_from_stdout()
// ─────────────────────────────────────────────
bool	CgiHandler::read_from_stdout()
{
	char buf[CGI_CHUNK];
	ssize_t	n = read(_stdout_fd[READ_END], buf, sizeof(buf));

	if (n > 0)
	{
		_output.append(buf, n);
		return true;
	}
	else if (n == 0)
	{
		LOG_CGI_I() << "EOF: read from stdout complete!";
		_stdout_done = true;
		close_stdout_fd();
		return false;
	}
	else
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return true;
		LOG_CGI_E() << "Read form stdout failed: " << strerror(errno);
		_stdout_done = true;
		return false;
	}
}

// ─────────────────────────────────────────────
// check_child_status()
// ─────────────────────────────────────────────
void	CgiHandler::check_child_status()
{
	if (_pid <= 0 || _state != CGI_RUNNING)
		return;

	if (time(NULL) - _start_time > CGI_TIMEOUT_SECONDS)
	{
		LOG_CGI_W() << "pid=" << _pid << " times out, killing";
		kill_child();
		_state = CGI_TIMEOUT;
		return;
	}

	int status;
	pid_t	ret = waitpid(_pid, &status, WNOHANG);
	if (ret == _pid)
	{
		if (_stdout_done)
		{
			_state = CGI_DONE;
			LOG_CGI_D() << "pid=" << _pid << " exited normally, status=" << status;
		}
		else
		{
			// _state = CGI_ERROR;
        	LOG_CGI_W() << "pid=" << _pid << " exited but stdout not done, status=" << status;
		}
	}
}
void	CgiHandler::kill_child()
{
	if (_pid > 0)
	{
		kill(_pid, SIGKILL);
		int status;
		waitpid(_pid, &status, 0);
		_pid = -1;
	}
	close_stdin_fd();
	close_stdout_fd();
}
