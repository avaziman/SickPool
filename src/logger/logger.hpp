#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <mutex>
#include <fmt/color.h>
#include <fmt/format.h>

#include <cstdio>

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
    RoundManager,
    Benchmark,

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
        case LogField::RoundManager:
            return "ROUND";
        case LogField::Benchmark:
            return "BENCH";
        default:
            return "Unknown";
    }
}

class Logger
{
   public:
    static std::mutex log_mutex;

    template <typename... T>
    static void Log(LogType type, LogField field,
                    fmt::format_string<T...> message, T&&... args)
    {
        std::scoped_lock lock(log_mutex);
        const char* field_str = ToString(field);
        switch (type)
        {
            case LogType::Debug:
                fmt::print(fg(fmt::color::green), "[DEBUG][{}] ", field_str);
                break;
            case LogType::Info:
                fmt::print(fg(fmt::color::dodger_blue), "[INFO][{}] ", field_str);
                break;
            case LogType::Warn:
                fmt::print(fg(fmt::color::yellow), "[WARN][{}] ", field_str);
                break;
            case LogType::Error:
                fmt::print(fg(fmt::color::red), "[ERROR][{}] ", field_str);
                break;
            case LogType::Critical:
                fmt::print(fg(fmt::color::red), "[CRITICAL][{}] ", field_str);
                break;
        }

        fmt::print(message, args...);
        printf("\n");
    }
};

#endif