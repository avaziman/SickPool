#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <mutex>
#include <fmt/color.h>
#include <fmt/format.h>
#include <cstdio>
#include "utils.hpp"

enum class LogType
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
    DaemonManager,
    Redis,
    StatsManager,
    ControlServer,
    SubmissionManager,
    PaymentManager,
    DiffManager,
    RoundManager,
    Benchmark,
    BlockWatcher,
    BlockSubmitter,
    Server,
};

template <LogField field>
class Logger
{
   public:
    mutable std::mutex log_mutex;

    static constexpr std::string_view field_name = EnumName<field>();

    // Logger(){

    // }

    template <LogType type, typename... T>
    /*static*/ inline void Log(fmt::format_string<T...> message, T&&... args) const
    {
        std::scoped_lock lock(log_mutex);
        fmt::v9::text_style color;
        switch (type)
        {
            case LogType::Debug:
                color = fg(fmt::color::green);
                break;
            case LogType::Info:
                color = fg(fmt::color::dodger_blue);
                break;
            case LogType::Warn:
                color = fg(fmt::color::yellow);
                break;
            case LogType::Error:
                color = fg(fmt::color::red);
                break;
            case LogType::Critical:
                color = fg(fmt::color::red);
                break;
        }

        fmt::print(color, fmt::format("[{}][{}]{}\n", EnumName<type>(), field_name, message), std::forward<T>(args)...);
        printf("\n");
    }
};

#endif