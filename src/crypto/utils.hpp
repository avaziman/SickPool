#ifndef UTILS_HPP_
#define UTILS_HPP_
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <sys/time.h>

#include "../config.hpp"
#include "verushash/arith_uint256.h"
#include "verushash/endian.h"
#include "verushash/uint256.h"

#define DIFF_US(end, start) duration_cast<microseconds>(end - start).count()
#define TIME_NOW() std::chrono::steady_clock::now()

// bool IsAddressValid(std::string addr){
//     std::vector<unsigned char> bytes;

//     // if(!DecodeBase58(addr, bytes) return false;
// }

inline int64_t GetCurrentTimeMs() { struct timeval tv;
    struct timeval time_now;
    gettimeofday(&time_now, nullptr);
    return (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
}

inline int fast_atoi(const char* str, int size)
{
    int val = 0;
    for (int i = 0; i < size; i++)
    {
        val = val * 10 + str[i] - '0';
    }
    return val;
}

inline void SetHighPriorityThread(std::thread& thr)
{
    struct sched_param param;
    int maxPriority;

    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    int res = pthread_setschedparam(thr.native_handle(), SCHED_FIFO, &param);
    if (res != 0)
        std::cerr << "Failed to set thread priority to realtime! (need admin)"
                  << std::endl;
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

inline uint32_t HexToUint(const char* hex, int size)
{
    uint32_t val = 0;
    for (int i = 0; i < size; i++)
    {
        val *= 16;
        val += GetHex(hex[i]);
    }
    return val;
}

inline void Hexlify(char* dest, unsigned char* src, int srcSize)
{
    const char hex[] = "0123456789abcdef";

    // each byte is 2 characters in hex
    for (int i = 0; i < srcSize; i++)
    {
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

inline void Unhexlify(unsigned char* dest, const char* src, int size)
{
    // each byte is 2 characters in hex
    for (int i = 0; i < size / 2; i++)
    {
        unsigned char char1 = GetHex(src[i * 2]);
        unsigned char char2 = GetHex(src[i * 2 + 1]);
        dest[i] = char2 + char1 * 16;
    }
}

inline void ToHex(char* dest, uint32_t num)
{
    // always puts nullchar at the end
    snprintf(dest, sizeof(uint32_t) * 2 + 1, "%08x", num);
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
inline std::vector<unsigned char> GetNumScript(const int64_t& value)
{
    std::vector<unsigned char> result;
    const bool neg = value < 0;
    uint64_t absvalue = neg ? -value : value;

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

// https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer
inline char VarInt(uint64_t& len)
{
    if (len < 0xfd)
        return 1;
    else if (len <= 0xffffffff)
    {
        len = (0xfd << 16) | len;
        return 3;
    }
    else if (len <= 0xffffffff)
    {
        len = ((uint64_t)0xfe << 32) | len;
        return 5;
    }

    // len = ((uint64_t)0xff << 64) | len;
    return 9;
}

inline char GetByteAmount(uint32_t num)
{
    if (num <= 0xff)
        return 1;
    else if (num <= 0xffff)
        return 2;
    else if (num <= 0xffffff)
        return 3;
    else if (num <= 0xffffffff)
        return 4;
}

// inline std::string VarInt(uint64_t len)
// {
//     std::stringstream ss;
//     if (len < 0xfd)
//         ss << std::hex << std::setfill('0') << std::setw(2) << len;
//     else if (len <= 0xffffffff)
//         ss << "fd" << std::hex << std::setfill('0') << std::setw(4)
//            << htobe16(len);
//     else if (len <= 0xffffffff)
//         ss << "fe" << std::hex << std::setfill('0') << std::setw(8)
//            << htobe32(len);
//     else
//         ss << "ff" << std::hex << std::setfill('0') << std::setw(16)
//            << htobe64(len);

//     return ss.str();
// }

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

inline double difficulty(const unsigned bits)
{
    const unsigned exponent_diff = 8 * (0x20 - ((bits >> 24) & 0xFF));
    const double significand = bits & 0xFFFFFF;
    return std::ldexp(0x0f0f0f / significand, exponent_diff);
}

// ms
inline int64_t GetDailyTimestamp()
{
    int64_t time = std::time(nullptr);
    time = time - (time % 86400);
    return time * 1000;
}

static double BitsToDiff(int64_t nBits)
{
    // from chain params

    int nShift = (nBits >> 24) & 0xff;
    int nShiftAmount = (DIFF1_BITS >> 24) & 0xff;

    double dDiff =
        (double)(DIFF1_BITS & 0x00ffffff) / (double)(nBits & 0x00ffffff);
    while (nShift < nShiftAmount)
    {
        dDiff *= 256.0;
        nShift++;
    }

    while (nShift > nShiftAmount)
    {
        nShift--;
        dDiff /= 256.0;
    }

    return dDiff;
}

// https://bitcoin.stackexchange.com/questions/30467/what-are-the-equations-to-convert-between-bits-and-difficulty
static uint32_t DiffToBits(double difficulty)
{
    int shiftBytes;
    int64_t word;
    for (shiftBytes = 1; true; shiftBytes++)
    {
        word =
            ((DIFF1_BITS & 0x00FFFFFF) * pow(0x100, shiftBytes)) / difficulty;
        if (word >= 0xffff) break;
    }
    word &= 0xffffff;  // convert to int < 0xffffff
    int size = (DIFF1_BITS >> 24) - shiftBytes;
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
// static std::string DiffToTarget(double diff)
// {
//     uint64_t t = 0x0000ffff00000000 / 1;

//     arith_uint256 arith256;
//     return arith256.SetCompact(t).GetHex();
// }

#endif