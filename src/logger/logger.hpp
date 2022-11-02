#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <mutex>
#include <fmt/color.h>
#include <fmt/format.h>
#include <cstdio>

enum class LogType
{
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Critical = 4,
};

template <std::string_view const& field_str>
class Logger
{
   public:
    Logger() = default;
    Logger(const Logger& _logger) {}
    mutable std::mutex log_mutex;

    // Logger(){

    // }

    template <LogType type, typename... T>
    /*static*/ inline void Log(fmt::format_string<T...> message, T&&... args) const
    {
        using enum fmt::color;

        std::scoped_lock lock(log_mutex);
        fmt::v9::text_style color;
        switch (type)
        {
            case LogType::Debug:
                color = fg(green);
                fmt::print(color, "[DEBUG]");
                break;
            case LogType::Info:
                color = fg(dodger_blue);
                fmt::print(color, "[INFO]");
                break;
            case LogType::Warn:
                color = fg(yellow);
                fmt::print(color, "[WARN]");
                break;
            case LogType::Error:
                color = fg(red);
                fmt::print(color, "[ERROR]");
                break;
            case LogType::Critical:
                color = fg(red);
                fmt::print(color, "[CRITICAL]");
                break;
        }

        fmt::print(color, "[{}] ", field_str);
        fmt::print(message, std::forward<T>(args)...);
        fmt::print("\n");
    }
};

#endif