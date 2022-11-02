#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <chrono>
#include <iostream>
#include <typeinfo>

#include "logger.hpp"

template <typename T>
class Benchmark
{
   public:
    Benchmark(std::string name)
        : start(std::chrono::steady_clock::now()), name(name)
    {
    }
    ~Benchmark()
    {
        auto end = std::chrono::steady_clock::now();
        T duration = std::chrono::duration_cast<T>(end - start);
        // logger.Log<LogType::Debug>( "Benchmark,
        //             "Bench \"{}\" took: {} units", name, duration.count());
    }

   private:
    std::string name;
    std::chrono::time_point<std::chrono::steady_clock> start;
};

#endif
