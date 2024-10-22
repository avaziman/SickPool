#ifndef UTILS_HPP_
#define UTILS_HPP_
#include <sys/time.h>

#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "static_config/static_config.hpp"
#include "utils/hex_utils.hpp"
#include "verushash/arith_uint256.h"

#define DIFF_US(end, start) \
    std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
#define TIME_NOW() std::chrono::steady_clock::now()

#define unlikely_cond(expr) (__builtin_expect(!!(expr), 0))
#define likely_cond(expr) (__builtin_expect(!!(expr), 1))

template <StaticConf confs>
constexpr auto GetDifficultyHex(const double diff)
{
    // how many hashes to find a share, lower target = more hashes
    const double raw_diff = confs.DIFF1 * (1.0 / diff);

    auto bin_target = DoubleToBin256(raw_diff);
    auto hex_target = Hexlify(bin_target);
    return hex_target;
}

// Helper function that converts a character to lowercase on compile time
constexpr char charToLower(const char c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

template <std::string_view const&... Strs>
struct join
{
    // Join all strings into a single std::array of chars
    static constexpr auto impl() noexcept
    {
        // add space for ';'
        constexpr std::size_t len = (Strs.size() + ...) + (sizeof...(Strs)) - 1;
        std::array<char, len + 1> arrr{};
        auto append = [i = 0, &arrr](auto const& s) mutable
        {
            for (char c : s)
            {
                arr[i++] = c;
            }
            arr[i++] = ':';
        };
        (append(Strs), ...);
        arrr[len] = 0;
        return arrr;
    }
    // Give the joined string static storage
    static constexpr auto arr = impl();
    // View as a std::string_view
    static constexpr std::string_view value{arr.data(), arr.size() - 1};
};
// Helper to get the value out
template <std::string_view const&... Strs>
static constexpr auto join_v = join<Strs...>::value;

// based on teos
template <auto T>
constexpr std::string_view EnumName()
{
    std::string_view name(std::source_location::current().function_name());
    auto var_pos = name.find('=') + 2;
    name = name.substr(var_pos,
                       std::min(name.find(';'), name.find(']')) - var_pos);
    // shortened
    name = name.substr(name.find("::") + 2);

    return name;
}

inline uint64_t GetCurrentTimeUs()
{
    struct timeval time_now;
    gettimeofday(&time_now, nullptr);
    return (time_now.tv_sec * 1000 * 1000) + (time_now.tv_usec);
}

inline uint64_t GetCurrentTimeMs() { return GetCurrentTimeUs() / 1000; }

inline int fast_atoi(const char* str, int size)
{
    int val = 0;
    for (int i = 0; i < size; i++)
    {
        val = val * 10 + str[i] - '0';
    }
    return val;
}

inline int SetHighPriorityThread(std::jthread& thr)
{
    struct sched_param param;

    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    int res = pthread_setschedparam(thr.native_handle(), SCHED_FIFO, &param);

    return res;
}

constexpr const char* BoolToCstring(const bool b)
{
    return b ? "true" : "false";
}

// from script.h
inline std::vector<uint8_t> GenNumScript(const int64_t value)
{
    std::vector<uint8_t> result;
    const bool neg = value < 0;
    uint64_t absvalue = neg ? -value : value;

    if (value == 0)
    {
        result.push_back(0);
        return result;
    }

    while (absvalue)
    {
        result.push_back(absvalue & 0xff);
        absvalue >>= 8;
    }

    //    - If the most significant byte is >= 0x80 and the value is positive,
    //    push a new zero-byte to make the significant byte < 0x80 again.

    //    - If the most significant byte is >= 0x80 and the value is negative,
    //    push a new 0x80 byte that will be popped off when converting to an
    //    integral.

    //    - If the most significant byte is < 0x80 and the value is negative,
    //    add 0x80 to it, since it will be subtracted and interpreted as a
    //    negative when converting to an integral.

    if (result.back() & 0x80)
        result.push_back(neg ? 0x80 : 0);
    else if (neg)
        result.back() |= 0x80;

    return result;
}

inline std::pair<uint64_t, uint8_t> ReadNumScript(uint8_t* script)
{
    uint8_t type = script[0];
    if (type < 0xFD)
    {
        return std::make_pair(type, 1);
    }
    else if (type == 0xFD)
    {
        return std::make_pair(*reinterpret_cast<uint16_t*>(script + 1), 3);
    }
    else if (type == 0xFE)
    {
        return std::make_pair(*reinterpret_cast<uint32_t*>(script + 1), 5);
    }
    else if (type == 0xFF)
    {
        return std::make_pair(*reinterpret_cast<uint64_t*>(script + 1), 9);
    }
    return std::make_pair(0, 0);
}

// https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer
inline char VarInt(uint64_t& len)
{
    if (len < 0xfd)
        return 1;
    else if (len <= 0xFFFF)
    {
        len = (0xfd << 16) | len;
        return 3;
    }
    else if (len <= 0xFFFFFFFF)
    {
        len = ((uint64_t)0xfe << 32) | len;
        return 5;
    }

    return 9;
}

inline uint8_t GetByteAmount(uint32_t num)
{
    if (num <= 0xff)
        return 1;
    else if (num <= 0xffff)
        return 2;
    else if (num <= 0xffffff)
        return 3;
    return 4;
}

inline int intPow(int x, unsigned int p)
{
    if (p == 0) return 1;
    if (p == 1) return x;

    int tmp = intPow(x, p / 2);
    if (p % 2 == 0)
        return tmp * tmp;
    else
        return x * tmp * tmp;
}

constexpr uint64_t pow2(int power)
{
    // if (power == 0) return 1;
    // return 2 * pow2(power - 1);

    return (1ULL << power);
}

constexpr double pow2d(int power)
{
    double res = 1.0;
    for (int i = 0; i < power; i++) res *= 2;

    return res;
}

template <StaticConf confs>
constexpr double GetHashMultiplier()
{
    constexpr double u256_max = pow2d(256);
    constexpr double hash_multiplier = u256_max / confs.DIFF1;

    return hash_multiplier;
    // for verus 2^ 24 / 0x0f0f0f = 17...
}

inline std::string ToLowerCase(std::string_view s){
    std::string lc;
    lc.reserve(s.size());
    std::ranges::transform(s, std::back_inserter(lc),
                           [](char c) { return std::tolower(c); });
    return lc;
}
#endif