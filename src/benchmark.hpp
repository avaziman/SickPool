#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <chrono>
#include <iostream>
#include <typeinfo>

#include "logger.hpp"
static constexpr std::string_view logger_name = "BENCHMARK";

template <typename T>
class Benchmark
{
   public:
    explicit Benchmark(std::string_view name)
        : start(std::chrono::steady_clock::now()), name(name)
    {
    }
    ~Benchmark()
    {
        auto end = std::chrono::steady_clock::now();
        T duration = std::chrono::duration_cast<T>(end - start);
        logger.Log<LogType::Debug>("Bench \"{}\" took: {} units", name, duration.count());
    }

   private:
    static inline Logger logger{logger_name};
    std::string name;
    std::chrono::time_point<std::chrono::steady_clock> start;
};

#endif
