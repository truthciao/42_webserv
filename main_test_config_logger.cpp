#include <iostream>
#include <exception>
#include <vector>
#include <set>
#include <map>

// 引入你的头文件（路径请根据实际情况调整）
#include "includes/ConfigParser.hpp" 
#include "includes/Config.hpp"       
#include "includes/Logger.hpp" // 引入你的 Logger


// c++ -Wall -Wextra -Werror -std=c++98 main_test_config_logger.cpp srcs/config/ConfigParser.cpp srcs/config/Config.cpp srcs/core/Logger.cpp -I includes -o webserv_test
//./webserv_test config/test.conf


// ==========================================
// 辅助打印函数：将内存中的结构体数据打印到终端
// 注意：对于大段的数据 Dump，依然保留 std::cout 比较清晰，
// 但我们在外层会用 Logger 记录它的执行状态。
// ==========================================
void debugPrintConfig(const Config& config) {
    const std::vector<ServerConfig>& servers = config.getServers();

    std::cout << "\n========== [PARSED CONFIGURATION] ==========\n";
    for (size_t i = 0; i < servers.size(); ++i) {
        std::cout << "\n[Server " << i + 1 << "]" << std::endl;
        std::cout << "  Host: " << servers[i].host << std::endl;
        std::cout << "  Port: " << servers[i].port << std::endl;
        std::cout << "  Client Max Body Size: " << servers[i].client_max_body_size << " bytes\n";
        
        std::cout << "  Server Names: ";
        for (size_t n = 0; n < servers[i].server_names.size(); ++n)
            std::cout << servers[i].server_names[n] << " ";
        std::cout << std::endl;

        for (size_t j = 0; j < servers[i].locations.size(); ++j) {
            const LocationConfig& loc = servers[i].locations[j];
            std::cout << "  [Location " << j + 1 << ": " << loc.route << "]" << std::endl;
            std::cout << "    Root: " << loc.root << std::endl;
            std::cout << "    Autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;
        }
        std::cout << "--------------------------------------------\n";
    }
}

// ==========================================
// 主函数
// ==========================================
int main(int argc, char **argv) {
    // 1. 全局初始化 Logger
    Logger::instance().set_use_color(true);
    Logger::instance().set_level(LOG_DEBUG); // 开启 DEBUG 级别以便看到最详尽的信息
    // Logger::instance().set_output_file("webserv.log"); // 如果需要双写到文件可以取消注释

    std::string config_file = (argc == 2) ? argv[1] : "default.conf";

    // 2. 使用 INFO 级别记录服务器启动和解析开始
    LOG_SERVER_I() << "Webserv is starting...";
    LOG_CONFIG_I() << "Attempting to parse configuration file: " << config_file;

    try {
        Config config;
        ConfigParser parser(config_file);
        
        // 执行解析
        parser.parse(config);
        
        // 3. 解析成功：使用 INFO 级别
        LOG_CONFIG_I() << "Parse successful! No syntax errors found.";
        
        // 4. 使用 DEBUG 级别记录一些内部统计信息
        LOG_CONFIG_D() << "Total server blocks successfully loaded: " << config.getServers().size();

        // 打印具体内容
        debugPrintConfig(config);

    } 
    catch (const std::exception& e) {
        // 5. 捕获所有解析异常：使用 ERROR 级别，精准报错
        LOG_CONFIG_E() << "Fatal Error during parsing: " << e.what();
        
        LOG_SERVER_W() << "Webserv shutdown due to invalid configuration.";
        return 1;
    }

    LOG_SERVER_I() << "Initialization complete. Ready to setup sockets.";
    return 0;
}