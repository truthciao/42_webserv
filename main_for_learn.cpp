#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

/*当你用 Ctrl+C 强行终止程序时，你的服务器其实是主动关闭连接的一方。为了防止最后一次挥手的确认包（ACK）在网络中丢包，或者防止旧连接的残留网络报文“幽灵般”地误入新连接，
操作系统的 TCP/IP 协议栈会强制让该端口在 TIME_WAIT 状态下保持生存一段时间（通常是 $2 \times MSL$，即两倍的最大报文段寿命）。

最标准、最优雅的解决办法是：在 bind 之前，设置 Socket 的属性，允许端口“地址复用”（SO_REUSEADDR）。*/

int main()
{
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		std::cerr << "Socket creation failed!" << std::endl;
		return 1;
	}

	int opt = 1;
    // SO_REUSEADDR 允许服务器快速重启，强行复用处于 TIME_WAIT 的端口
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Setsockopt SO_REUSEADDR failed!" << std::endl;
        close(server_fd);
        return 1;
    }

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(12345);

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
	while (1) {
		struct sockaddr_in cliend_address;
		socklen_t client_addr_len = sizeof(cliend_address);
	
		int	new_socket = accept(server_fd, (struct sockaddr *)&cliend_address, & client_addr_len);
		if (new_socket < 0)
		{
			std::cerr << "Accept failed!" << std::endl;
			close(server_fd);
			return 1;
		}
		std::cout << "\n--- Connection established! ---\n" << std::endl;
	
		char buffer[1024] = {0};
		read(new_socket, buffer, 1024);
	
		std::cout << "Browser sent the following HTTP Request:\n" << std::endl;
		std::cout << buffer << std::endl;
	
		const char *http_response =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 46\r\n"
			"\r\n"
			"<html><body><h1>Hello WebServ!</h1></body></html>";
	
		write(new_socket, http_response, strlen(http_response));
		close(new_socket);
	}
	close(server_fd);

	return (0);
}
