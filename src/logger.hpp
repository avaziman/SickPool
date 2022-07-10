#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <cstdio>
#include <fmt/format.h>

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

enum class LogField
{
    Config,
    Stratum,
    JobManager,
    ShareProcessor,
    Redis,
    StatsManager,
    ControlServer,
    SubmissionManager,
    PaymentManager,
    DiffManager,

};

inline const char* ToString(LogField v)
{
    switch (v)
    {
        case LogField::Config:
            return "CONFIG";
        case LogField::Stratum:
            return "STRATUM";
        case LogField::Redis:
            return "REDIS";
        case LogField::ShareProcessor:
            return "SHARES";
        case LogField::JobManager:
            return "JOBS";
        case LogField::StatsManager:
            return "STATS";
        case LogField::ControlServer:
            return "CONTROL";
        case LogField::SubmissionManager:
            return "SUBMISSION";
        case LogField::PaymentManager:
            return "PAYMENT";
        case LogField::DiffManager:
            return "DIFF";
        default:
            return "Unknown";
    }
}

class Logger
{
   public:
    template <typename... T>
     static void Log(LogType type, LogField field, fmt::format_string<T...> message,
                    T&&... args)
    {
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

        // if constexpr (sizeof...(args))
        // {
            fmt::print(message, args...);
        // // }
        // else
        // {
        //     fmt::print(message);
        // }
        printf("\n");
    }
};

#endif