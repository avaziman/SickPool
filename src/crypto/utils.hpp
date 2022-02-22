#ifndef UTILS_HPP_
#define UTILS_HPP_
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "verushash/arith_uint256.h"
#include "verushash/endian.h"
#include "verushash/uint256.h"

// bool IsAddressValid(std::string addr){
//     std::vector<unsigned char> bytes;

//     // if(!DecodeBase58(addr, bytes) return false;
// }

inline std::string ReverseHex(std::string input)
{
    int size = input.size();
    char str[size + 1];
    for (int i = 0; i < size / 2; i += 2)
    {
        char left = input[i];
        char left1 = input[i + 1];
        char right = input[size - (i + 1)];
        char right1 = input[size - (i + 2)];

        str[i + 1] = right;
        str[i] = right1;
        str[size - (i + 2)] = left;
        str[size - (i + 1)] = left1;
    }
    str[size] = '\0';
    return std::string(str);
}

inline char* ReverseHex(const char* input, uint16_t size)
{
    char* str = new char[size + 1];
    for (int i = 0; i < size / 2; i += 2)
    {
        char left = input[i];
        char left1 = input[i + 1];
        char right = input[size - (i + 1)];
        char right1 = input[size - (i + 2)];

        str[i + 1] = right;
        str[i] = right1;
        str[size - (i + 2)] = left;
        str[size - (i + 1)] = left1;
    }
    str[size] = '\0';
    return str;
}

inline std::string Unhexlify(const std::string& hex)
{
    std::vector<unsigned char> bytes;

    for (unsigned int i = 0; i < hex.length(); i += 2)
    {
        std::string byteString = hex.substr(i, 2);
        unsigned char byte =
            (unsigned char)strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }

    std::string result(bytes.begin(), bytes.end());
    return result;
}

constexpr const char* BoolToCstring(const bool b) { return b ? "true" : "false"; }

inline char GetHex(char c)
{
    // assumes all hex is in lowercase
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');

    return 0;
}

inline void HexlifyLE(unsigned char* src, int srcSize, char* res)
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
        res[i * 2] = c1;
        res[i * 2 + 1] = c2;
    }
}

inline void Unhexlify(unsigned char* arr, int size)
{
    // each byte is 2 characters in hex
    for (int i = 0; i < size / 2; i++)
    {
        unsigned char char1 = GetHex(arr[i * 2]);
        unsigned char char2 = GetHex(arr[i * 2 + 1]);
        arr[i] = char1 + char2 * 16;
    }
}

inline void Unhexlify(char* src, int size, unsigned char* dest)
{
    // each byte is 2 characters in hex
    for (int i = 0; i < size / 2; i++)
    {
        unsigned char char1 = GetHex(src[i * 2]);
        unsigned char char2 = GetHex(src[i * 2 + 1]);
        dest[i] = char1 + char2 * 16;
    }
}

inline std::string ToHex(uint32_t num)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << htobe32(num);
    return ss.str();
}

inline void ToHex(char* dest, uint32_t num) { sprintf(dest, "%08x", num); }

inline uint32_t FromHex(std::string str)
{
    uint32_t val;
    std::stringstream ss;
    ss << std::hex << str;
    ss >> val;
    return val;
}

// https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer
inline std::string VarInt(uint64_t len)
{
    std::stringstream ss;
    if (len < 0xfd)
        ss << std::hex << std::setfill('0') << std::setw(2) << len;
    else if (len <= 0xffffffff)
        ss << "fd" << std::hex << std::setfill('0') << std::setw(4)
           << htobe16(len);
    else if (len <= 0xffffffff)
        ss << "fe" << std::hex << std::setfill('0') << std::setw(8)
           << htobe32(len);
    else
        ss << "ff" << std::hex << std::setfill('0') << std::setw(16)
           << htobe64(len);

    return ss.str();
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

static double DifficultyFromBits(int64_t nBits)
{
    // from chain params
    uint256 powLimit256 = uint256S(
        "0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
    uint32_t powLimit = UintToArith256(powLimit256).GetCompact(false);
    int nShift = (nBits >> 24) & 0xff;
    int nShiftAmount = (powLimit >> 24) & 0xff;

    double dDiff =
        (double)(powLimit & 0x00ffffff) / (double)(nBits & 0x00ffffff);
    while (nShift < nShiftAmount)
    {
        dDiff *= 256.0;
        nShift++;
    }

    while (nShift > nShiftAmount)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

static std::string DiffToTarget(double diff)
{
    uint64_t t = 0x0000ffff00000000 / 1;

    arith_uint256 arith256;
    return arith256.SetCompact(t).GetHex();
}

#endif