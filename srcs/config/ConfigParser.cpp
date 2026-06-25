#include "../../includes/ConfigParser.hpp"

ConfigParser::ConfigParser() : _configFile(""), _pos(0) {}

ConfigParser::ConfigParser(const std::string& file) : _configFile(file), _pos(0) {}

ConfigParser::ConfigParser(const ConfigParser& src) {
	*this = src;
}

ConfigParser& ConfigParser::operator=(const ConfigParser& rhs) {
	if (this != &rhs) {
		this->_configFile = rhs._configFile;
		this->_tokens = rhs._tokens;
		this->_pos = rhs._pos;
	}
	return *this;
}

ConfigParser::~ConfigParser() {}



//main function
void ConfigParser::parse(Config& config) {
	std::ifstream file(_configFile.c_str());
	if (!file.is_open())
		throw std::runtime_error("Cannot open config file: " + _configFile);

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	_tokenize(content);

	while (_pos < _tokens.size()) {
		std::string	token = _accept();
		if (token == "server") {
			config.addServer(_parseServer());
		} else {
			throw std::runtime_error("Unknown directive in global context: " +token);
		}
	}
}


//tokenizer
void ConfigParser::_tokenize(const std::string& content) {
	std::string spaced_content;

	// 1. 预处理：在特殊符号前后加空格
	for (size_t i = 0; i < content.length(); ++i) {
		char c = content[i];
		if (c == '{' || c == '}' || c == ';') {
			spaced_content += ' ';
			spaced_content += c;
			spaced_content += ' ';
		}
		else if (c == '#') {
			// 如果遇到注释符号 '#'，忽略当前行剩下的所有内容
			while (i < content.length() && content[i] != '\n')
				i++;
		}
		else {
			spaced_content += c;
		}
	}

	// 2. 利用 stringstream 自动按空格切分单词
	std::stringstream ss(spaced_content);
	std::string token;
	while (ss >> token) {
		_tokens.push_back(token);
	}
}


//parser
ServerConfig ConfigParser::_parseServer()
{
	ServerConfig server;
	LOG_CONFIG_D() << "Entering _parseServer() block...";

	_expect("{");

	while (_pos < _tokens.size() && _tokens[_pos] != "}") {
		std::string directive = _accept();

		if (directive == "listen") {
			std::string arg = _accept();
			std::string ip_str = "";
			std::string port_str = "";

			// 如果参数中包含 localhost，将其替换为 127.0.0.1
            size_t localhost_pos = arg.find("localhost");
            if (localhost_pos != std::string::npos) {
                arg.replace(localhost_pos, 9, "127.0.0.1");
            }

			// --- 路由分配: 拆分 IP 和 Port ---
            size_t colon_pos = arg.find(':');
            if (colon_pos != std::string::npos) {
                // 情况 C: 包含冒号，说明是 IP:Port 格式 (例如 127.0.0.1:80)
                ip_str = arg.substr(0, colon_pos);
                port_str = arg.substr(colon_pos + 1);
            }
            else if (arg.find('.') != std::string::npos) {
                // 情况 B: 没有冒号，但有小数点，说明只有 IP (例如 192.168.1.1)
                ip_str = arg;
                // port_str 保持为空，意味着使用开头的默认端口 8080
            }
            else {
                // 情况 A: 既没冒号也没小数点，说明只有端口 (例如 80)
                // ip_str 保持为空，意味着使用开头的默认 IP 0.0.0.0
                port_str = arg;
            }

			// --- 赋值与校验 ---
			if (!ip_str.empty()) {
				if (!_isValidIP(ip_str)) {
					throw std::invalid_argument("Invalid IP format: " + ip_str);
				}
				server.host = ip_str;
			}

			if (!port_str.empty()) {
				if (!_isValidPort(port_str)) {
					throw std::invalid_argument("Invalid or restricted port: " + port_str);
				}
				server.port = std::atoi(port_str.c_str());
			}
			LOG_CONFIG_D() << "Parsed listen directive: " << server.host << ":" << server.port;
			_expect(";");
		}

		else if (directive == "host") {
			server.host = _accept();
			if (server.host == "localhost")
				server.host = "127.0.0.1";
			else if (!server.host.empty()) {
				if (!_isValidIP(server.host)) {
					throw std::invalid_argument("Invalid IP format: " + server.host);
				}
			}
			LOG_CONFIG_D() << "Parsed host: " << server.host;

			_expect(";");
		}

		else if (directive == "port") {
			std::string port_arg = _accept();
			if (!port_arg.empty()) {
				if (!_isValidPort(port_arg)) {
					throw std::invalid_argument("Invalid or restricted port: " + port_arg);
				}
				server.port = std::atoi(port_arg.c_str());
			}
			LOG_CONFIG_D() << "Parsed port: " << server.port;

			_expect(";");
		}

		else if (directive == "server_name") {
			bool has_arg = false;//标记是否有至少一个域名
			while (_pos < _tokens.size() && _tokens[_pos] != ";") {
				std::string name = _accept();
				if (!_isValidServerName(name)) {
					throw std::invalid_argument("Invalid server_name format: " + name);
				}
				server.server_names.push_back(name);
				has_arg = true;
			}
			if (!has_arg) {
				throw std::invalid_argument("Server_name requires at least one argument.");
			}
			LOG_CONFIG_D() << "Found server_name";

			_expect(";");
		}

		else if (directive == "client_max_body_size") {
			server.client_max_body_size = _parseBodySize(_accept());
			LOG_CONFIG_D() << "Parsed client_max_body_size: " << server.client_max_body_size;

			_expect(";");
		}

		else if (directive == "error_page") {
			std::string code_str = _accept();
			if (!_isValidErrorCode(code_str)) {
				throw std::invalid_argument("Invalid error code (must be 300-599): " + code_str);
			}
			int code = std::atoi(code_str.c_str());

			std::string path = _accept();
			if (path.empty()) {
				throw std::invalid_argument("Error page path (cannot be empty).");
			}
			if (path[0] != '/') {
				throw std::invalid_argument("Error page path (should start with '/'): " + path);
			}
			server.error_pages[code] = path;
			LOG_CONFIG_D() << "Found error_page";
			_expect(";");
		}

		else if (directive == "root") {
			server.root = _accept();
			if (server.root == ";") throw std::invalid_argument("Directive 'root' has no argument.");
			_expect(";");
		}

		else if (directive == "cgi") {
			std::string ext = _accept();
			if (ext[0] != '.') {
				throw std::invalid_argument("CGI extension must start with a dot (.), got: " + ext);
			}
			std::string path = _accept();
			if (path == ";") {
				throw std::invalid_argument("Missing CGI path for extention");
			}
			server.cgi_ext_path[ext] = path;
			_expect(";");
		}

		else if (directive == "location") {
			LOG_CONFIG_D() << "Entering _parseLocation() block...";

			server.locations.push_back(_parseLocation(server));

			LOG_CONFIG_D() << "_parseLocation() block finished.";
		}

		else {
			throw std::runtime_error("Unknown server directive: " + directive);
		}
	}

	_expect("}");

	return server;
}

LocationConfig ConfigParser::_parseLocation(ServerConfig& server)
{
	LocationConfig loc;

	loc.client_max_body_size = server.client_max_body_size;
	loc.root = server.root;
	loc.cgi_ext_path = server.cgi_ext_path;

	std::string route = _accept();
	if (route == "{" || route == ";") {
		throw std::invalid_argument("Location is missing a route path.");
	}
	if (route[0] != '/') {
		throw std::invalid_argument("Location route must start with '/': " + route);
	}
	loc.route = route;
	LOG_CONFIG_D() << "Parsed route: " << loc.route;

	_expect("{");

	while (_pos < _tokens.size() && _tokens[_pos] != "}") {
		std::string directive = _accept();

		if (directive == "root") {
			loc.root = _accept();
			if (loc.root == ";") throw std::invalid_argument("Directive 'root' has no argument.");
			_expect(";");
		}

		else if (directive == "index") {
			loc.index = _accept();
			if (loc.index == ";") throw std::invalid_argument("Directive 'index' has no argument.");
			_expect(";");
		}

		else if (directive == "client_max_body_size") {
			loc.client_max_body_size = _parseBodySize(_accept());
			_expect(";");
		}

		else if (directive == "autoindex") {
			std::string value = _accept();
			if (value != "on" && value != "off")
				throw std::invalid_argument("autonidex must be 'on' or 'off', got: " + value);
			loc.autoindex = (value == "on");
			_expect(";");
		}

		else if (directive == "allow_methods") {
			bool has_methods = false;
			while (_pos < _tokens.size() && _tokens[_pos] != ";") {
				std::string method = _accept();
				if (method != "GET" && method != "POST" && method != "DELETE") {
					throw std::invalid_argument("Invalid or unsupported HTTP method: " + method);
				}
				loc.allow_methods.insert(method);//塞入 std::set
				has_methods = true;
			}
			if (!has_methods) {
                throw std::invalid_argument("allow_methods requires at least one method.");
			}
			_expect(";");
		}

		else if (directive == "return") {
			std::string code_str = _accept();
			if (!_isValidErrorCode(code_str)) {
				throw std::invalid_argument("Invalid redirect code: " + code_str);
			}
			loc.redirect_code = std::atoi(code_str.c_str());

			loc.redirect_url = _accept();
			if (loc.redirect_url == ";") {
				throw std::invalid_argument("Missing redirect URL after code");
			}
			_expect(";");
		}

		else if (directive == "upload_enable") {
			std::string value = _accept();
            if (value != "on" && value != "off")
                throw std::invalid_argument("upload_enable must be 'on' or 'off', got: " + value);
            loc.upload_enable = (value == "on");
            _expect(";");
		}

		else if (directive == "upload_store") {
			loc.upload_store = _accept();
			if (loc.upload_store == ";") {
				throw std::invalid_argument("Directive 'upload_store' has no argument.");
			}
			_expect(";");
		}

		else if (directive == "cgi") {
			std::string ext = _accept();
			if (ext[0] != '.') {
				throw std::invalid_argument("CGI extension must start with a dot (.), got: " + ext);
			}
			std::string path = _accept();
			if (path == ";") {
				throw std::invalid_argument("Missing CGI path for extention");
			}
			loc.cgi_ext_path[ext] = path;
			_expect(";");
		}

		else {
			throw std::runtime_error("Unknown location directive: " + directive);
		}
	}

	_expect("}");

	return loc;
}


//helpers
std::string ConfigParser::_accept()
{
	if (_pos >= _tokens.size())
		throw std::runtime_error("Unexpected EOF");

	return _tokens[_pos++];
}

void ConfigParser::_expect(const std::string& token)
{
	if (_pos >= _tokens.size() || _tokens[_pos] != token)
		throw std::runtime_error("Expected token: " + token);

	_pos++;
}

bool ConfigParser::_isValidIP(const std::string& ip) const {
	int dot_count = 0;
	std::string temp = "";

	for (size_t i = 0; i < ip.length(); ++i) {
		if (ip[i] == '.') {
			dot_count++;
			//数字段不能为空，长度不超过3
			if (temp.empty() || temp.length() > 3) return false;
			//数字本身0～255
			int num = std::atoi(temp.c_str());
			if (num < 0 || num > 255) return false;
			temp = "";//清空，准备下一段
		}
		else if (std::isdigit(ip[i])) {
			temp += ip[i];
		}
		else {
			return false;//出现非法字符
		}
	}
	//检查最后一段
	if (temp.empty() || temp.length() > 3) return false;
	int num = std::atoi(temp.c_str());
	if (num < 0 || num > 255) return false;
	//dot = 3
	return dot_count == 3;
}

bool ConfigParser::_isValidPort(const std::string& port_str) const {
	if (port_str.empty() || port_str.length() > 5) return false;

	//必须是数字
	for (size_t i = 0; i < port_str.length(); ++i) {
		if (!std::isdigit(port_str[i])) return false;
	}

	//转换并检查
	long port_val = std::strtol(port_str.c_str(), NULL, 10);
	if (port_val < 1 || port_val > 65535) return false;

	return true;
}

bool ConfigParser::_isValidServerName(const std::string& name) const {
    // 1. 长度防线：不能为空，且不能超过 DNS 规范的 253 个字符
    if (name.empty() || name.length() > 253) {
        return false;
    }

    // 2. 字符防线：只能是字母、数字、点(.)、连字符(-)或下划线(_)
    for (size_t i = 0; i < name.length(); ++i) {
        char c = name[i];
        if (!std::isalnum(c) && c != '.' && c != '-' && c != '_') {
            return false;
        }
    }

    // 进阶检查：域名不能以点或连字符开头/结尾
    if (name[0] == '.' || name[0] == '-' ||
        name[name.length() - 1] == '.' || name[name.length() - 1] == '-') {
        return false;
    }

    return true;
}

bool ConfigParser::_isValidErrorCode(const std::string& code_str) const {
    // 状态码通常是 3 位数
    if (code_str.length() != 3) return false;

    for (size_t i = 0; i < code_str.length(); ++i) {
        if (!std::isdigit(code_str[i])) return false;
    }

    int code = std::atoi(code_str.c_str());
    // 错误码通常在 300 - 599 之间
    if (code < 300 || code > 599) return false;

    return true;
}

size_t ConfigParser::_parseBodySize(const std::string& size_str) const {
    // 1. 第一道防线：判空
    if (size_str.empty()) {
        throw std::invalid_argument("client_max_body_size cannot be empty");
    }

    // 2. 第二道防线：严格的“全数字”检查
    for (size_t i = 0; i < size_str.length(); ++i) {
        if (!std::isdigit(size_str[i])) {
            throw std::invalid_argument("Client_max_body_size only accepts pure numbers (bytes). Got: " + size_str);
        }
    }

    // 3. 第三道防线：转换并检查内存溢出
    // 在调用 strtoul 之前，先将系统的错误码重置为 0
    errno = 0;
    unsigned long raw_val = std::strtoul(size_str.c_str(), NULL, 10);

    // strtoul 非常聪明，如果数字大到溢出，它会返回 ULONG_MAX 并把 errno 设为 ERANGE
    if (errno == ERANGE) {
        throw std::out_of_range("client_max_body_size value is too large and caused overflow: " + size_str);
    }

    return static_cast<size_t>(raw_val);
}
