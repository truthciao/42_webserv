#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

# include "Config.hpp"
# include <string>
# include <vector>
# include <fstream>
# include <sstream>
# include <iostream>
# include <stdexcept>

class ConfigParser {
private:
    std::vector<std::string> _tokens;  // 存放切分好的所有单词
    size_t                   _pos;     // 当前读到了第几个单词

public:
    ConfigParser();
    ConfigParser(const ConfigParser& src);
    ConfigParser& operator=(const ConfigParser& rhs);
    ~ConfigParser();

    Config parseFile(const std::string& filename);

private:
    // tokenizer
    void tokenizeFile(const std::string& filename);

    // parser
    Config parseConfig();
    ServerConfig parseServerBlock();
    LocationConfig parseLocationBlock();

    // helpers
    bool hasMoreTokens() const;
    std::string peek() const;
    std::string get();

    void expect(const std::string& token);

};

#endif