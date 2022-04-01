#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <cstdio>

enum ColorCode
{
    FG_RED = 31,
    FG_GREEN = 32,
    FG_YELLOW = 33,
    FG_BLUE = 34,
    FG_DEFAULT = 39,
    BG_RED = 41,
    BG_GREEN = 42,
    BG_BLUE = 44,
    BG_DEFAULT = 49
};

enum LogType
{
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Critical = 4,
};

enum LogField
{
    Config = 0,
    Stratum = 1,
    ShareProcessor = 2,
    Redis = 3,
};

inline const char* ToString(LogField v)
{
    switch (v)
    {
        case Config:
            return "Config";
        case Stratum:
            return "Stratum";
        case Redis:
            return "Redis";
        case ShareProcessor:
            return "ShareProcessor";
        default:
            return "Unknown";
    }
}

class Logger
{
   public:
    template <typename... Args>
    static void Log(LogType type, LogField field, const char* message, Args... args){
        
        const char* field_str = ToString(field);
        switch (type)
        {
            // make this printf
            case LogType::Debug:
                printf("\033[1;%dm[DEBUG][%s] \033[0m", FG_GREEN, field_str);
                break;
            case LogType::Info:
                printf("\033[1;%dm[INFO][%s] \033[0m", FG_BLUE, field_str);
                break;
            case LogType::Warn:
                printf("\033[1;%dm[WARN][%s] \033[0m", FG_YELLOW, field_str);
                break;
            case LogType::Error:
                printf("\033[1;%dm[ERROR][%s] \033[0m", FG_RED, field_str);
                break;
            case LogType::Critical:
                printf("\033[1;%dm[CRITICAL][%s] \033[0m", FG_RED, field_str);
                break;
        }

        printf(message, args...);
        printf("\n");
    }
};

#endif