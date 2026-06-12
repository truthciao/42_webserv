#include "Logger.hpp"

Logger &Logger::instance()
{
    static Logger inst;
    return inst;
}

Logger::Logger()
    : min_level_(LOG_DEBUG)
    , use_color_(true)
    , file_enabled_(false)
{
}

Logger::~Logger()
{
    if (file_stream_.is_open())
        file_stream_.close();
}

void Logger::set_level(LogLevel level)   { min_level_ = level; }
void Logger::set_use_color(bool enabled) { use_color_ = enabled; }

void Logger::set_output_file(const std::string &filepath)
{
    if (file_stream_.is_open())
        file_stream_.close();
    file_stream_.open(filepath.c_str(), std::ios::out | std::ios::app);
    file_enabled_ = file_stream_.is_open();
}

void Logger::emit(LogModule module, LogLevel level, const std::string &msg)
{
    if (level < min_level_)
        return;

    std::string ts  = get_timestamp();
    std::string mod = module_to_string(module);

    if (use_color_)
    {
        std::string out =
            std::string(WHITE) + ts + RESET
            + " " + module_color(module) + "[" + mod + "]" + RESET
            + " " + level_color(level) + msg + RESET + "\n";

        if (level >= LOG_WARNING)
            std::cerr << out;
        else
            std::cout << out;
    }
    else
    {
        std::string out = ts + " [" + mod + "] " + msg + "\n";
        if (level >= LOG_WARNING)
            std::cerr << out;
        else
            std::cout << out;
    }

    if (file_enabled_)
    {
        file_stream_ << ts << " [" << mod << "] " << msg << "\n";
        file_stream_.flush();
    }
}

std::string Logger::get_timestamp() const
{
    time_t     now = time(NULL);
    struct tm *t   = localtime(&now);
    char buf[32];
    std::sprintf(buf, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    return std::string(buf);
}

std::string Logger::module_to_string(LogModule mod) const
{
    switch (mod)
    {
        case MOD_SERVER:   return "SERVER  ";
        case MOD_CLIENT:   return "CLIENT  ";
        case MOD_REQUEST:  return "REQUEST ";
        case MOD_RESPONSE: return "RESPONSE";
        case MOD_CONFIG:   return "CONFIG  ";
        case MOD_CGI:      return "CGI     ";
        case MOD_FILE:     return "FILE    ";
        case MOD_SOCKET:   return "SOCKET  ";
        default:           return "UNKNOWN ";
    }
}

std::string Logger::module_color(LogModule mod) const
{
    switch (mod)
    {
        case MOD_SERVER:   return GREEN;
        case MOD_CLIENT:   return M_LAVENDER;
        case MOD_REQUEST:  return BLUE;
        case MOD_RESPONSE: return MAGENTA;
        case MOD_CONFIG:   return YELLOW;
        case MOD_CGI:      return WHITE;
        case MOD_FILE:     return CYAN;
        case MOD_SOCKET:   return GREEN;
        default:           return WHITE;
    }
}

std::string Logger::level_color(LogLevel level) const
{
    switch (level)
    {
        case LOG_DEBUG:   return GRAY;
        case LOG_INFO:    return "";
        case LOG_WARNING: return YELLOW;
        case LOG_ERROR:   return RED;
        default:          return "";
    }
}
