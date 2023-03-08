#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>

enum class LogType
{
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Critical = 4,
};

class Logger
{
   public:
    static std::string GetNowString()
    {
        const auto now = std::chrono::system_clock::now();
        return fmt::format("{:%y%m%d-%H:%M:%OS}", now);
    }

    const std::string log_folder = "./logs";

    explicit Logger(std::string_view field_str)
    {
        std::string folder_name = fmt::format("{}/{}", log_folder, field_str);
        std::string file_name = folder_name + "/" + GetNowString();

        try
        {
            std::filesystem::create_directory(log_folder);
            std::filesystem::create_directory(folder_name);
        }
        catch (const std::filesystem::filesystem_error& err)
        {
        }

        log_stream.open(file_name);

        // if (!log_stream.good())
        // {
        //     throw std::invalid_argument("Failed to open log file: {}", file_name);
        // }
    }

    mutable std::ofstream log_stream;
    mutable std::mutex log_mutex;

    template <LogType type, typename... T>
    /*static*/ inline void Log(fmt::format_string<T...> message,
                               T&&... args) const
    {
        using namespace fmt;
        using enum fmt::color;
        std::scoped_lock lock(log_mutex);
        v9::text_style color;
        const char* prefix = nullptr;

        switch (type)
        {
            case LogType::Debug:
                color = fg(green);
                prefix = "DEBUG";
                break;
            case LogType::Info:
                color = fg(dodger_blue);
                prefix = "INFO";
                break;
            case LogType::Warn:
                color = fg(yellow);
                prefix = "WARN";
                break;
            case LogType::Error:
                color = fg(red);
                prefix = "ERROR";
                break;
            case LogType::Critical:
                color = fg(red);
                prefix = "CRITICAL";
                break;
        }

        log_stream << GetNowString() << " " << prefix << " ";
        print(log_stream, message, std::forward<T>(args)...);
        print(log_stream, "{}", "\n");

#ifdef DEBUG
        std::cout << GetNowString() << " " << prefix << " ";

        print(message, std::forward<T>(args)...);
        print("{}", "\n");
#endif
    }
};

#endif