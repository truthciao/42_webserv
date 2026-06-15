#include <iostream>
#include <exception>
#include <vector>
#include <set>
#include <map>
// 这里的路径请根据你的实际项目结构调整
#include "includes/ConfigParser.hpp" 
#include "includes/Config.hpp"       


//c++ -Wall -Wextra -Werror -std=c++98 main_test_config.cpp srcs/config/ConfigParser.cpp srcs/config/Config.cpp -I includes -o webserv_test
// ==========================================
// 辅助打印函数：将内存中的结构体数据打印到终端
// ==========================================
void debugPrintConfig(const Config& config) {
    // 假设你的 Config 类里有一个 getServers() 方法返回 std::vector<ServerConfig>
    // 如果你是设为 public 变量，可以直接 config.servers
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

        std::cout << "  Error Pages: " << std::endl;
        for (std::map<int, std::string>::const_iterator it = servers[i].error_pages.begin(); 
             it != servers[i].error_pages.end(); ++it) {
            std::cout << "    " << it->first << " -> " << it->second << std::endl;
        }

        for (size_t j = 0; j < servers[i].locations.size(); ++j) {
            const LocationConfig& loc = servers[i].locations[j];
            std::cout << "\n  [Location " << j + 1 << ": " << loc.route << "]" << std::endl;
            std::cout << "    Root: " << loc.root << std::endl;
            std::cout << "    Index: " << loc.index << std::endl;
            std::cout << "    Autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;
            
            std::cout << "    Allow Methods: ";
            for (std::set<std::string>::const_iterator it = loc.allow_methods.begin(); 
                 it != loc.allow_methods.end(); ++it) {
                std::cout << *it << " ";
            }
            std::cout << std::endl;

            if (loc.redirect_code != 0) {
                std::cout << "    Redirect: " << loc.redirect_code << " -> " << loc.redirect_url << std::endl;
            }

            std::cout << "    Upload Enable: " << (loc.upload_enable ? "on" : "off") << std::endl;
            if (!loc.upload_store.empty())
                std::cout << "    Upload Store: " << loc.upload_store << std::endl;

            std::cout << "    CGI Extensions: " << std::endl;
            for (std::map<std::string, std::string>::const_iterator it = loc.cgi_ext_path.begin(); 
                 it != loc.cgi_ext_path.end(); ++it) {
                std::cout << "      " << it->first << " -> " << it->second << std::endl;
            }
        }
        std::cout << "--------------------------------------------\n";
    }
}

// ==========================================
// 主函数
// ==========================================
int main(int argc, char **argv) {
    // 1. 获取配置文件路径，默认使用 default.conf
    std::string config_file = (argc == 2) ? argv[1] : "default.conf";

    try {
        // 2. 实例化全局配置对象
        Config config;
        
        // 3. 实例化解析器并传入文件路径
        ConfigParser parser(config_file);

        std::cout << "=> Starting to parse: " << config_file << " ..." << std::endl;
        
        // 4. 执行核心解析逻辑
        parser.parse(config);
        
        std::cout << "=> Parse successful! No syntax errors found." << std::endl;

        // 5. 打印验证内存中的数据
        debugPrintConfig(config);

    } 
    catch (const std::exception& e) {
        // 你的所有 throw std::invalid_argument / runtime_error 都会被捕获到这里
        std::cerr << "\n[ConfigParser Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}