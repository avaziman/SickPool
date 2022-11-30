#ifndef HEX_UTILS_HPP
#define HEX_UTILS_HPP
#include <array>
#include <iomanip>
#include <iostream>
#include <string>

constexpr auto u64maxd = static_cast<double>(UINT64_MAX);

#define BIN_TO_DOUBLE_8B(bin)                                             \
    const uint64_t* bin8 = reinterpret_cast<const uint64_t*>(bin.data()); \
    static_assert(sizeof(bin) % 8 == 0, "Incompatible byte length");      \
    double res = 0.0;                                                     \
    for (int i = 0; i < sizeof(bin) / sizeof(uint64_t); i++)              \
    {                                                                     \
        res *= u64maxd;                                                   \
        res += static_cast<double>(bin8[i]);                              \
    }                                                                     \
    return res

#define BIN_TO_DOUBLE_1B(bin)               \
    double res = 0.0;                       \
    for (int i = 0; i < sizeof(bin); i++)   \
    {                                       \
        res *= 256.0;                       \
        res += static_cast<double>(bin[i]); \
    }                                       \
    return res

// log 256 n complexity
constexpr static std::array<uint8_t, 32> DoubleToBin256(double d)
{
    std::array<uint8_t, 32> res{};

    int i = 0;
    while (d >= 1.0)
    {
        double remain = fmod(d, 256.0);
        // little endian, least significant first
        res[sizeof(res) - i - 1] = static_cast<uint8_t>(remain);

        d /= 256.0;
        i++;
    }

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

constexpr char GetHex(char c)
{
    // assumes all hex is in lowercase
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    else if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');

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

template <size_t size>
constexpr std::array<char, size * 2> Hexlify(
    const std::array<uint8_t, size>& src)
{
    std::array<char, size * 2> res;
    Hexlify(res.data(), src.data(), size);
    return res;
}

template <size_t size>
constexpr std::string HexlifyS(const std::array<uint8_t, size>& src)
{
    std::string res;
    res.resize(size * 2);
    Hexlify(res.data(), src.data(), size);
    return res;
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

constexpr void Unhexlify(unsigned char* dest, const char* src,
                         const size_t size)
{
    // each byte is 2 characters in hex
    for (int i = 0; i < size / 2; i++)
    {
        unsigned char char1 = GetHex(src[i * 2]);
        unsigned char char2 = GetHex(src[i * 2 + 1]);
        dest[i] = char2 + char1 * 16;
    }
}

template <size_t size>
constexpr std::array<uint8_t, size / 2> Unhexlify(
    const std::array<char, size>& src)
{
    std::array<uint8_t, size / 2> res{};
    Unhexlify(res.data(), src.data(), size);
    return res;
}

inline std::string UnhexlifyS(std::string_view src)
{
    std::string res;
    res.resize(src.size() / 2);
    Unhexlify(reinterpret_cast<uint8_t*>(res.data()), src.data(), src.size());
    return res;
}

template <size_t size>
constexpr static double BytesToDouble(const std::array<uint8_t, size>& bin)
{
    // BIN_TO_DOUBLE_8B(bin);
    BIN_TO_DOUBLE_1B(bin);
}

template <const std::string_view& hex>
constexpr static double HexToDouble()
{
    std::array<char, hex.size()> src{};
    std::ranges::copy(hex, src.begin());
    std::array<uint8_t, hex.size() / 2> bin = Unhexlify(src);

    BIN_TO_DOUBLE_1B(bin);
}

#endif