#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <errno.h>

bool set_nonblocking(int fd)
{
	if(fcntl(fd, F_SETFL, O_NONBLOCK) == 1)
	{
		std::cerr << "fcntl F_SETFL O_NONBLOCK failed\n";
		return false;
	}
	return true;
}

int main()
{
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		std::cerr << "Socket creation failed!" << std::endl;
		return 1;
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
		return (1);
	}

	if (listen(server_fd, 3) < 0)
	{
		std::cerr << "Listen Failed!" << std::endl;
		close(server_fd);
		return (1);
	}
    std::cout << "Server is listening on port 8080..." << std::endl;

	while (true)
	{
		struct sockaddr_in cliend_address;
		socklen_t client_addr_len = sizeof(cliend_address);

		int	client_fd = accept(server_fd, (struct sockaddr *)&cliend_address, & client_addr_len);
		if (client_fd < 0)
		{
			std::cerr << "Accept failed!" << std::endl;
			close(server_fd);
			return 1;
		}
		std::cout << "\n--- Connection established! ---\n" << std::endl;

		char buffer[1024] = {0};
		read(client_fd, buffer, 1024);

		std::cout << "Browser sent the following HTTP Request:\n" << std::endl;
		std::cout << buffer << std::endl;

		const char *http_response =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 46\r\n"
			"\r\n"
			"<html><body><h1>Hello WebServ!</h1></body></html>";

		write(client_fd, http_response, strlen(http_response));

		close(client_fd);
	}

	close(server_fd);

	return (0);
}
