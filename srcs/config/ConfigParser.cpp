#include "../../includes/ConfigParser.hpp"

ConfigParser::ConfigParser() : _pos(0) {}

ConfigParser::ConfigParser(const ConfigParser& src) {
    *this = src;
}

ConfigParser& ConfigParser::operator=(const ConfigParser& rhs) {
    if (this != &rhs) {
        this->_tokens = rhs._tokens;
        this->_pos = rhs._pos;
    }
    return *this;
}

ConfigParser::~ConfigParser() {}

Config ConfigParser::parseFile(const std::string& filename)
{
    tokenizeFile(filename);

    return parseConfig();
}


//tokenizer
void ConfigParser::tokenizeFile(const std::string& filename)
{
    std::ifstream file(filename.c_str());

    if (!file)
        throw std::runtime_error("Cannot open config file");

    _tokens.clear();

    std::string line;

    while (std::getline(file, line))
    {
        size_t comment = line.find('#');

        if (comment != std::string::npos)
            line.erase(comment);

        std::string current;

        for (size_t i = 0; i < line.size(); i++)
        {
            char c = line[i];

            if (c == '{' || c == '}' || c == ';')
            {
                if (!current.empty())
                {
                    _tokens.push_back(current);
                    current.clear();
                }

                _tokens.push_back(std::string(1, c));
            }
            else if (std::isspace(c))
            {
                if (!current.empty())
                {
                    _tokens.push_back(current);
                    current.clear();
                }
            }
            else
            {
                current += c;
            }
        }

        if (!current.empty())
            _tokens.push_back(current);
    }

    _pos = 0;
}


//parser
Config ConfigParser::parseConfig()
{
    Config config;

    while (hasMoreTokens())
    {
        if (peek() != "server")
            throw std::runtime_error("Expected server block");

        config.addServer(parseServerBlock());
    }

    return config;
}

ServerConfig ConfigParser::parseServerBlock()
{
    ServerConfig server;

    get();          // server
    expect("{");

    while (peek() != "}")
    {
        std::string directive = get();

        if (directive == "host")
        {
            server.host = get();
            expect(";");
        }

        else if (directive == "port")
        {
            server.port = std::atoi(get().c_str());
            expect(";");
        }

        else if (directive == "server_name")
        {
            while (peek() != ";")
                server.server_names.push_back(get());

            expect(";");
        }

        else if (directive == "client_max_body_size")
        {
            server.client_max_body_size =
                std::strtoul(get().c_str(), NULL, 10);

            expect(";");
        }

        else if (directive == "error_page")
        {
            int code = std::atoi(get().c_str());
            std::string path = get();

            server.error_pages[code] = path;

            expect(";");
        }

        else if (directive == "location")
        {
            LocationConfig loc = parseLocationBlock();
            server.locations.push_back(loc);
        }

        else
        {
            throw std::runtime_error(
                "Unknown server directive: " + directive);
        }
    }

    expect("}");

    return server;
}

LocationConfig ConfigParser::parseLocationBlock()
{
    LocationConfig loc;

    loc.route = get();

    expect("{");

    while (peek() != "}")
    {
        std::string directive = get();

        if (directive == "root")
        {
            loc.root = get();
            expect(";");
        }

        else if (directive == "index")
        {
            loc.index = get();
            expect(";");
        }

        else if (directive == "autoindex")
        {
            std::string value = get();

            loc.autoindex = (value == "on");

            expect(";");
        }

        else if (directive == "allow_methods")
        {
            while (peek() != ";")
                loc.allow_methods.insert(get());

            expect(";");
        }

        else if (directive == "return")
        {
            loc.redirect_code =
                std::atoi(get().c_str());

            loc.redirect_url = get();

            expect(";");
        }

        else if (directive == "upload_enable")
        {
            loc.upload_enable = (get() == "on");
            expect(";");
        }

        else if (directive == "upload_store")
        {
            loc.upload_store = get();
            expect(";");
        }

        else if (directive == "cgi")
        {
            std::string ext = get();
            std::string path = get();

            loc.cgi_ext_path[ext] = path;

            expect(";");
        }

        else
        {
            throw std::runtime_error(
                "Unknown location directive: " + directive);
        }
    }

    expect("}");

    return loc;
}


//helpers
bool ConfigParser::hasMoreTokens() const
{
    return (_pos < _tokens.size());
}

std::string ConfigParser::peek() const
{
    if (!hasMoreTokens())
        throw std::runtime_error("Unexpected EOF");

    return _tokens[_pos];
}

std::string ConfigParser::get()
{
    std::string token = peek();
    _pos++;
    return token;
}

void ConfigParser::expect(const std::string& token)
{
    if (get() != token)
        throw std::runtime_error("Expected token: " + token);
}