#ifndef UTILS_HPP_
#define UTILS_HPP_
#include <sys/time.h>

#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

#include "static_config/static_config.hpp"
#include "verushash/arith_uint256.h"
#include "verushash/endian.h"
#include "verushash/uint256.h"

#define DIFF_US(end, start) \
    std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
#define TIME_NOW() std::chrono::steady_clock::now()

#define unlikely_cond(expr) (__builtin_expect(!!(expr), 0))
#define likely_cond(expr) (__builtin_expect(!!(expr), 1))

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
        constexpr std::size_t len =
            (Strs.size() + ...) + (sizeof...(Strs)) - 1;
        std::array<char, len + 1> arr{};
        auto append = [i = 0, &arr](auto const& s) mutable
        {
            for (char c : s)
            {
                arr[i++] = c;
            }
            arr[i++] = ':';
        };
        (append(Strs), ...);
        arr[len] = 0;
        return arr;
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
    // std::string_view name(std::source_location::current().function_name());
    std::string_view name(std::source_location::current().function_name());
    auto var_pos = name.find('=') + 2;
    name = name.substr(var_pos,
                       std::min(name.find(';'), name.find(']')) - var_pos);
    // shortened
    name = name.substr(name.find("::") + 2);

    // for (int i = 0; i < name.size(); i ++)
    // {
    //     // replace underscored with -, and make lowercase
    //     char* c = (char*)(name.data() + i);
    //     if (*c == '_') *c = '-';
    //     *c = charToLower(*c);
    // }
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

inline int SetHighPriorityThread(std::thread& thr)
{
    struct sched_param param;
    int maxPriority;

    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    int res = pthread_setschedparam(thr.native_handle(), SCHED_FIFO, &param);

    return res;
}

inline void ReverseHex(char* dest, const char* input, uint32_t size)
{
    for (int i = 0; i < size / 2; i += 2)
    {
        char left = input[i];
        char left1 = input[i + 1];
        char right = input[size - (i + 1)];
        char right1 = input[size - (i + 2)];

        dest[i + 1] = right;
        dest[i] = right1;
        dest[size - (i + 2)] = left;
        dest[size - (i + 1)] = left1;
    }
}

constexpr const char* BoolToCstring(const bool b)
{
    return b ? "true" : "false";
}

inline char GetHex(char c)
{
    // assumes all hex is in lowercase
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');

    return 0;
}

inline uint64_t HexToUint(const char* hex, std::size_t size)
{
    uint64_t val = 0;
    for (int i = 0; i < size; i++)
    {
        val *= 16;
        val += GetHex(hex[i]);
    }
    return val;
}

template <typename T>
constexpr void Hexlify(char* dest, const T* src, size_t srcSize)
{
    constexpr const char hex[] = "0123456789abcdef";

    // each byte is 2 characters in hex
    for (int i = 0; i < srcSize; i++)
    {
        // unsigned char val = src[i];
        unsigned char val = src[i];

        char c1 = '0', c2 = '0';
        if (val < 16)
            c2 = hex[val];
        else
        {
            char c2Val = val % 16;
            c2 = hex[c2Val];
            c1 = hex[(val - c2Val) / 16];
        }
        dest[i * 2] = c1;
        dest[i * 2 + 1] = c2;
    }
}

template <const std::string_view& src>
constexpr auto Hexlify()
{
    constexpr std::size_t hex_size = src.size() * 2;
    std::array<char, hex_size> arr;
    Hexlify(arr.data(), src.data(), src.size());

    return arr;
}

inline void PrintHex(const uint8_t* b, std::size_t size,
                     std::string comment = "")
{
    std::cout << comment << ": ";
    for (int i = 0; i < size; i++)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << int(b[i]);
    }
    std::cout << std::endl;
}

inline void Unhexlify(unsigned char* dest, const char* src, const size_t size)
{
    // each byte is 2 characters in hex
    for (int i = 0; i < size / 2; i++)
    {
        unsigned char char1 = GetHex(src[i * 2]);
        unsigned char char2 = GetHex(src[i * 2 + 1]);
        dest[i] = char2 + char1 * 16;
    }
}

inline uint32_t FromHex(const char* str)
{
    uint32_t val;
    std::stringstream ss;
    ss << std::hex << str;
    ss >> val;
    return val;
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

    // problem
    //  len = ((uint64_t)0xff << 64) | len;
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

constexpr double BitsToDiff(const unsigned bits)
{
    const unsigned exponent_diff = 8 * (DIFF1_EXPONENT - ((bits >> 24) & 0xFF));
    const double significand = bits & 0xFFFFFF;
    return std::ldexp(DIFF1_COEFFICIENT / significand, exponent_diff);
}

constexpr uint64_t pow2(int power)
{
    // if (power == 0) return 1;

    // return 2 * pow2(power - 1);

    return (1ULL << power);
}

inline double GetExpectedHashes(const double diff)
{
    constexpr uint64_t power = 256 - 8 * (DIFF1_EXPONENT - 3);
    constexpr double hash_multiplier =
        static_cast<double>(pow2(power)) / DIFF1_COEFFICIENT;

    return diff * hash_multiplier;
    // for verus 2^ 24 / 0x0f0f0f = 17...
}

// ms
inline int64_t GetDailyTimestamp()
{
    int64_t time = std::time(nullptr);
    time = time - (time % 86400);
    return time * 1000;
}

// https://bitcoin.stackexchange.com/questions/30467/what-are-the-equations-to-convert-between-bits-and-difficulty
static uint32_t DiffToBits(double difficulty)
{
    int shiftBytes;
    int64_t word;
    for (shiftBytes = 1; true; shiftBytes++)
    {
        word = (DIFF1_COEFFICIENT * pow(0x100, shiftBytes)) / difficulty;
        if (word >= 0xffff) break;
    }
    word &= 0xffffff;  // convert to int < 0xffffff
    int size = DIFF1_EXPONENT - shiftBytes;
    // the 0x00800000 bit denotes the sign, so if it is already set, divide the
    // mantissa by 0x100 and increase the size by a byte
    if (word & 0x800000)
    {
        word >>= 8;
        size++;
    }
    uint32_t bits = (size << 24) | word;
    return bits;
}

#endif