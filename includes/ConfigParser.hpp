#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

# include "Config.hpp"
# include <string>
# include <vector>
# include <fstream>
# include <sstream>
# include <iostream>
# include <stdexcept>
# include <cctype>
# include <cstdlib>
# include <cerrno>
# include <climits>

class ConfigParser {
private:
    std::string              _configFile; //配置文件的路径
    std::vector<std::string> _tokens;  // 存放切分好的所有单词
    size_t                   _pos;     // 当前读到了第几个单词

public:
    ConfigParser();
    ConfigParser(const std::string& file);
    ConfigParser(const ConfigParser& src);
    ConfigParser& operator=(const ConfigParser& rhs);
    ~ConfigParser();

    //唯一主入口
    void parse(Config& config);

private:
    // tokenizer
    void _tokenize(const std::string& content);

    // parser
    ServerConfig _parseServer();
    LocationConfig _parseLocation();

    // helpers
    std::string _accept();
    void _expect(const std::string& token);

    bool _isValidIP(const std::string& ip) const;
    bool _isValidPort(const std::string& port_str) const;
    bool _isValidServerName(const std::string& name) const;
    bool _isValidErrorCode(const std::string& code_str) const;
    size_t _parseBodySize(const std::string& size_str) const;

};

#endif