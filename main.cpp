#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

bool set_nonblocking(int fd)
{
	if(fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
	{
		std::cerr << "fcntl F_SETFL O_NONBLOCK failed\n";
		return false;
	}
	return true;
}

int create_serve_socket(int port)
{
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
		return -1;
	}

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);

	if (bind(server_fd, (struct sockaddr * )&address, sizeof(address)) < 0)
	{
		std::cerr << "Bind failed! Port might be in use." << std::endl;
		close(server_fd);
		return -1;
	}

	if (listen(server_fd, 3) < 0)
	{
		std::cerr << "Listen Failed!" << std::endl;
		close(server_fd);
		return -1;
	}

	if (!set_nonblocking(server_fd))
	{
		close(server_fd);
		return -1;
	}

    std::cout << "Server listening on port " << port << " (non-blocking mode)" << std::endl;
	return server_fd;
}

void handle_new_connection(int server_fd, std::vector<struct pollfd> &poll_fds)
{
	while (true)
	{
		struct sockaddr_in client_address;
		socklen_t client_addr_len = sizeof(client_address);

		int	client_fd = accept(server_fd, (struct sockaddr *)&client_address, & client_addr_len);
		if (client_fd < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
            std::cerr << "Accept failed: " << strerror(errno) << std::endl;
			break;
		}

		if (!set_nonblocking(client_fd))
		{
			close(client_fd);
			continue;
		}

		struct pollfd pfd;
		pfd.fd = client_fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		poll_fds.push_back(pfd);

		std::cout << "[+] New connection: fd=" << client_fd
                  << " (total clients: " << poll_fds.size() - 1 << ")" << std::endl;
	}
}

void handle_client_data(int client_fd, std::vector<struct pollfd> &poll_fds, size_t index)
{
	char buffer[1024] = {0};
	ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

	if (bytes_read == 0)
	{
		std::cout << "[-] Client disconnected: fd=" << client_fd << std::endl;
		close(client_fd);
		poll_fds.erase(poll_fds.begin() + index);
		return;
	}
	else if (bytes_read < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			std::cerr << "[-] Read error on fd=" << client_fd << ": " << strerror(errno) << std::endl;
			close(client_fd);
			poll_fds.erase(poll_fds.begin() + index);
			return;
		}
	}

	std::cout << "Browser sent the following HTTP Request:\n" << std::endl;
	std::cout << buffer << std::endl;

	const char *http_response =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: 46\r\n"
		"\r\n"
		"<html><body><h1>Hello WebServ!</h1></body></html>";

	ssize_t bytes_written = write(client_fd, http_response, strlen(http_response));
	if (bytes_written < 0)
        std::cerr << "Write error on fd=" << client_fd << ": " << strerror(errno) << std::endl;

	close(client_fd);
	poll_fds.erase(poll_fds.begin() + index);
}

int main()
{
	const int	PORT = 8080;

	int server_fd = create_serve_socket(PORT);
	if (server_fd < 0)
		return (1);

	std::vector<struct pollfd> poll_fds;

	struct pollfd server_pfd;
	server_pfd.fd = server_fd;
	server_pfd.events = POLLIN;
	server_pfd.revents = 0;
	poll_fds.push_back(server_pfd);

	std::cout << "\n=== Server running with poll() ==="  << std::endl;
    std::cout << "Open http://localhost:" << PORT << " in your browser\n" << std::endl;

	while (true)
	{

		int ready = poll(poll_fds.data(), poll_fds.size(), -1);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "poll() failed: " << strerror(errno) << std::endl;
            break;
		}

		for (size_t i = poll_fds.size(); i > 0; --i)
		{
			size_t idx = i - 1;
			if (poll_fds[idx].revents == 0)
				continue;
			if (poll_fds[idx].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (poll_fds[idx].fd == server_fd)
				{
					std::cerr << "Server socket error!" << std::endl;
                    close(server_fd);
                    return 1;
				}
                std::cout << "[-] Error on fd=" << poll_fds[idx].fd << ", closing" << std::endl;
				close(poll_fds[idx].fd);
				poll_fds.erase(poll_fds.begin() + idx);
				continue;
			}

			if (poll_fds[idx].revents & POLLIN)
			{
				if (poll_fds[idx].fd == server_fd)
					handle_new_connection(server_fd, poll_fds);
				else
					handle_client_data(poll_fds[idx].fd, poll_fds, idx);
			}
		}


	}

	for (size_t i = 0; i < poll_fds.size(); ++i)
		close(poll_fds[i].fd);

	return (0);
}
