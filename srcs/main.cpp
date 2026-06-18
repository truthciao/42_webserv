#include "Server.hpp"
#include "Config.hpp"
#include "ConfigParser.hpp"
#include "Logger.hpp"

#include <csignal>

volatile sig_atomic_t g_running = 1;


void signal_handler(int signum)
{
    (void)signum;
    g_running = 0;
}

int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);
 	signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

	std::string config_path = (argc == 2) ? argv[1] : "config/default.conf";

	Config config;
	try
	{
		ConfigParser parser(config_path);
		parser.parse(config);
	}
	catch(const std::exception& e)
	{
		LOG_CONFIG_E() << "Failed to parse config: " << e.what();
		return 1;
	}

	Server server;

	if (!server.setup(config))
		return 1;

	server.run();

	return 0;
}
