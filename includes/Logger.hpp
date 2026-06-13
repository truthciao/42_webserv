#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <ctime>

enum LogLevel
{
    LOG_DEBUG   = 0,
    LOG_INFO    = 1,
    LOG_WARNING = 2,
    LOG_ERROR   = 3
};

enum LogModule
{
    MOD_SERVER,
    MOD_CLIENT,
    MOD_REQUEST,
    MOD_RESPONSE,
    MOD_CONFIG,
    MOD_CGI,
    MOD_FILE,
    MOD_SOCKET
};

class Logger
{
public:
    static Logger &instance();

    void set_level(LogLevel level);
    void set_output_file(const std::string &filepath);
    void set_use_color(bool enabled);

    // 被 LogStream 析构时调用
    void emit(LogModule module, LogLevel level, const std::string &msg);

private:
    Logger();
    ~Logger();
    Logger(const Logger &);
    Logger &operator=(const Logger &);

    std::string get_timestamp() const;
    std::string module_to_string(LogModule mod) const;
    std::string module_color(LogModule mod) const;
    std::string level_color(LogLevel level) const;

    LogLevel      min_level_;
    bool          use_color_;
    bool          file_enabled_;
    std::ofstream file_stream_;
};

// ─────────────────────────────────────────────
// LogStream: 临时对象，析构时自动输出
// ─────────────────────────────────────────────

class LogStream
{
public:
    LogStream(LogModule module, LogLevel level)
        : module_(module), level_(level) {}

    // 析构时把攒好的字符串交给 Logger
    ~LogStream()
    {
        Logger::instance().emit(module_, level_, ss_.str());
    }

    // 万能 << 重载
    template <typename T>
    LogStream &operator<<(const T &value)
    {
        ss_ << value;
        return *this;
    }

private:
    // 禁止拷贝（C++98 不会 RVO 出问题，但以防万一）
    LogStream(const LogStream &);
    LogStream &operator=(const LogStream &);

    LogModule          module_;
    LogLevel           level_;
    std::ostringstream ss_;
};

// ─────────────────────────────────────────────
// 宏：每次调用创建一个临时 LogStream 对象
// 用法：LOG_SERVER_I() << "listening on " << port;
// ─────────────────────────────────────────────

#define LOG_SERVER_D()   LogStream(MOD_SERVER,   LOG_DEBUG)
#define LOG_SERVER_I()   LogStream(MOD_SERVER,   LOG_INFO)
#define LOG_SERVER_W()   LogStream(MOD_SERVER,   LOG_WARNING)
#define LOG_SERVER_E()   LogStream(MOD_SERVER,   LOG_ERROR)

#define LOG_CLIENT_D()   LogStream(MOD_CLIENT,   LOG_DEBUG)
#define LOG_CLIENT_I()   LogStream(MOD_CLIENT,   LOG_INFO)
#define LOG_CLIENT_W()   LogStream(MOD_CLIENT,   LOG_WARNING)
#define LOG_CLIENT_E()   LogStream(MOD_CLIENT,   LOG_ERROR)

#define LOG_REQUEST_D()  LogStream(MOD_REQUEST,  LOG_DEBUG)
#define LOG_REQUEST_I()  LogStream(MOD_REQUEST,  LOG_INFO)
#define LOG_REQUEST_W()  LogStream(MOD_REQUEST,  LOG_WARNING)
#define LOG_REQUEST_E()  LogStream(MOD_REQUEST,  LOG_ERROR)

#define LOG_RESPONSE_D() LogStream(MOD_RESPONSE, LOG_DEBUG)
#define LOG_RESPONSE_I() LogStream(MOD_RESPONSE, LOG_INFO)
#define LOG_RESPONSE_W() LogStream(MOD_RESPONSE, LOG_WARNING)
#define LOG_RESPONSE_E() LogStream(MOD_RESPONSE, LOG_ERROR)

#define LOG_CONFIG_D()   LogStream(MOD_CONFIG,   LOG_DEBUG)
#define LOG_CONFIG_I()   LogStream(MOD_CONFIG,   LOG_INFO)
#define LOG_CONFIG_W()   LogStream(MOD_CONFIG,   LOG_WARNING)
#define LOG_CONFIG_E()   LogStream(MOD_CONFIG,   LOG_ERROR)

#define LOG_CGI_D()      LogStream(MOD_CGI,      LOG_DEBUG)
#define LOG_CGI_I()      LogStream(MOD_CGI,      LOG_INFO)
#define LOG_CGI_W()      LogStream(MOD_CGI,      LOG_WARNING)
#define LOG_CGI_E()      LogStream(MOD_CGI,      LOG_ERROR)

#define LOG_FILE_D()     LogStream(MOD_FILE,     LOG_DEBUG)
#define LOG_FILE_I()     LogStream(MOD_FILE,     LOG_INFO)
#define LOG_FILE_W()     LogStream(MOD_FILE,     LOG_WARNING)
#define LOG_FILE_E()     LogStream(MOD_FILE,     LOG_ERROR)

#define LOG_SOCKET_D()   LogStream(MOD_SOCKET,   LOG_DEBUG)
#define LOG_SOCKET_I()   LogStream(MOD_SOCKET,   LOG_INFO)
#define LOG_SOCKET_W()   LogStream(MOD_SOCKET,   LOG_WARNING)
#define LOG_SOCKET_E()   LogStream(MOD_SOCKET,   LOG_ERROR)

// ─────────────────────────────────────────────
// COLOR
// ─────────────────────────────────────────────

// reset
#define RESET      "\033[0m"              // 重置所有样式（颜色/加粗等恢复默认）

#define RED "\033[31m"     // 红色
#define GREEN "\033[32m"   // 绿色
#define YELLOW "\033[33m"  // 黄色
#define BLUE "\033[34m"    // 蓝色
#define WHITE  "\033[1;37m"
#define CYAN   "\033[1;36m"
#define MAGENTA  "\033[35m"
#define GRAY    "\033[0;90m"

// ==== soft (Morandi-like) colors ====
#define M_GREY     "\033[38;2;168;168;168m" // 柔和灰色
#define M_BEIGE    "\033[38;2;196;182;155m" // 米色（偏暖、低饱和）
#define M_SAGE     "\033[38;2;159;176;160m" // 鼠尾草绿（淡绿色）
#define M_BLUE     "\033[38;2;140;163;186m" // 柔和蓝色
#define M_LAVENDER "\033[38;2;180;162;190m" // 淡紫色（薰衣草色）
#define M_ROSE     "\033[38;2;190;150;150m" // 玫瑰粉（低饱和红）
#define M_PEACH    "\033[38;2;214;162;140m" // 蜜桃色（柔橙色）
#define M_MUSTARD  "\033[38;2;196;170;120m" // 芥末黄（偏暗黄）

// ==== slightly deeper tones ====
#define M_DGREY    "\033[38;2;110;110;110m" // 深灰色
#define M_DGREEN   "\033[38;2;110;140;120m" // 深绿色（偏灰）
#define M_DBLUE    "\033[38;2;100;130;160m" // 深蓝色（柔和）
#define M_DBROWN   "\033[38;2;140;110;90m"  // 棕色（低饱和）

// ==== utility ====
#define BOLD       "\033[1m"               // 加粗文本
#define DIM        "\033[2m"               // 变暗（降低亮度）
#define ITALIC     "\033[3m"               // 斜体（部分终端支持）

#endif
