#include "../../includes/Config.hpp"

// ==========================================
// LocationConfig 实现
// ==========================================

LocationConfig::LocationConfig() :
    route(""),
    root(""),
    index("index.html"),
    autoindex(false),
    client_max_body_size(1024 * 1024) ,
    redirect_code(0),
    redirect_url(""),
    upload_enable(false),
    upload_store("")
     {}

LocationConfig::LocationConfig(const LocationConfig& src) {
    *this = src;
}

LocationConfig& LocationConfig::operator=(const LocationConfig& rhs) {
    if (this != &rhs) {
        this->route = rhs.route;
        this->allow_methods = rhs.allow_methods;
        this->root = rhs.root;
        this->index = rhs.index;
        this->autoindex = rhs.autoindex;
        this->redirect_code = rhs.redirect_code;
        this->redirect_url = rhs.redirect_url;
        this->upload_enable = rhs.upload_enable;
        this->upload_store = rhs.upload_store;
        this->cgi_ext_path = rhs.cgi_ext_path;
        this->client_max_body_size = rhs.client_max_body_size;
    }
    return *this;
}

LocationConfig::~LocationConfig() {}

// ==========================================
// ServerConfig 实现
// ==========================================

ServerConfig::ServerConfig() :
    host("0.0.0.0"),
    port(8080),
    client_max_body_size(1024 * 1024) ,
    root("")
    {} // 默认限制 1MB

ServerConfig::ServerConfig(const ServerConfig& src) {
    *this = src;
}

ServerConfig& ServerConfig::operator=(const ServerConfig& rhs) {
    if (this != &rhs) {
        this->host = rhs.host;
        this->port = rhs.port;
        this->client_max_body_size = rhs.client_max_body_size;
        this->error_pages = rhs.error_pages;
        this->locations = rhs.locations;
        this->root = rhs.root;
        this->cgi_ext_path = rhs.cgi_ext_path;
    }
    return *this;
}

ServerConfig::~ServerConfig() {}

// ==========================================
// Config 实现
// ==========================================

Config::Config() {}

Config::Config(const Config& src) {
    *this = src;
}

Config& Config::operator=(const Config& rhs) {
    if (this != &rhs) {
        this->_servers = rhs._servers;
    }
    return *this;
}

Config::~Config() {}

void Config::addServer(const ServerConfig& server) {
    _servers.push_back(server);
}

const std::vector<ServerConfig>& Config::getServers() const {
    return _servers;
}
