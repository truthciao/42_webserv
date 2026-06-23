#ifndef CONFIG_HPP
# define CONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include <set>

/*设计两层结构：

LocationConfig：对应配置文件里的 location 块（路由级别配置）。
ServerConfig：对应配置文件里的 server 块（核心服务配置，包含多个路由）。
*/

// ==========================================
// 1. 路由配置结构体 (对应 Nginx 的 location 块)
// ==========================================

//在 C++ 中，当我们只想把一堆相关的变量“打包”在一起，方便传来传去，
//而不需要对这些数据进行任何保护或复杂的逻辑控制时，我们就会用 struct
struct LocationConfig {
    std::string route;                       // 路由匹配前缀，例如 "/images"
    std::set<std::string> allow_methods;  // 允许的方法，例如 "GET", "POST"
    std::string root;                        // 本地根目录映射，例如 "/tmp/www"
    std::string index;                       // 默认首页文件名，例如 "index.html"
    bool autoindex;                          // 是否开启目录列表 (Directory Listing)

    // HTTP 重定向
    int redirect_code;                       // 重定向状态码 (如 301, 302)，0 表示不重定向
    std::string redirect_url;                // 重定向的目标 URL

    // 文件上传
    bool upload_enable;                      // 是否允许该路由上传文件
    std::string upload_store;                // 上传文件保存的本地目录

    // CGI 配置 (扩展名 -> CGI可执行文件路径，如 ".py" -> "/usr/bin/python3")
    std::map<std::string, std::string> cgi_ext_path;

    LocationConfig();
    LocationConfig(const LocationConfig& src);
    LocationConfig& operator=(const LocationConfig& rhs);
    ~LocationConfig();
};

// ==========================================
// 2. 服务器配置类 (对应 Nginx 的 server 块)
// ==========================================
struct ServerConfig {
    std::string host;                        // 监听的网卡IP，例如 "127.0.0.1"
    int port;                                // 监听的端口，例如 8080
    std::vector<std::string> server_names;   // 域名/主机名， “localhost”, "webserv.42.fr"
    size_t client_max_body_size;             // 限制客户端请求体大小 (字节)
    std::map<int, std::string> error_pages;  // 错误码对应的错误页面路径 (如 404 -> "/404.html")
    std::vector<LocationConfig> locations;   // 该服务器下的所有路由块
    std::map<std::string, std::string> cgi_ext_path;

    ServerConfig();
    ServerConfig(const ServerConfig& src);
    ServerConfig& operator=(const ServerConfig& rhs);
    ~ServerConfig();
};

// ==========================================
// 3. 顶层配置容器 (管理整个配置文件的所有 server)
// ==========================================
class Config {
private:
    std::vector<ServerConfig> _servers;      // 存储配置文件里所有的 server 块

public:
    Config();
    Config(const Config& src);
    Config& operator=(const Config& rhs);
    ~Config();

    // 供给人员 A 和业务代码调用的底层接口
    void addServer(const ServerConfig& server);
    const std::vector<ServerConfig>& getServers() const;
};

#endif
