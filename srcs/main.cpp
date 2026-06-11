#include "Server.hpp"

#include <csignal>

volatile sig_atomic_t g_running = 1;

void signal_handler(int signum)
{
    (void)signum;
    g_running = 0;
}

int main()
{
 	signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

	const int PORT = 8080;

	Server server;

	if (!server.setup(PORT))
		return 1;

	server.run();

	return 0;
}
