#ifndef JSONIFY_HPP_
#define JSONIFY_HPP_

#include <array>
#include <charconv>
#include <string_view>
#include <vector>

#define STRINGIFY(x) #x
// template <typename T>
// struct json_t {

//     static constexpr std::array<
// };

// struct json_str : json_t
// {
//     template <std::string_view Sv>
//     constexpr json_str() : sv(s){}
//     std::string_view sv;

//      constexpr std::string ToString() override{
//         return std::string(sv.begin(), sv.end());
//     }
// };

// template <int const& i>
// consteval auto StrJson()
// {
//     constexpr size_t LEN = 10;
//     std::array<char, LEN> arr;

//     constexpr const char* s = STRINGIFY(i);
//     // constexpr auto res = std::to_chars(arr.data(), arr.data() + LEN, i);
//     // constexpr auto real_len = res.ptr - arr.data();

//     // return std::array<char, real_len>(arr.begin(), arr.begin() +
//     real_len);
// }

// template <int const& i>
// struct json_int_t
// {
//     static constexpr auto value = StrJson<i>();
// };

// template <std::string_view const& Sv>
// struct json_str_t
// {
//     static constexpr auto value = StrJson<Sv>();
// };

template <std::string_view const& Sv>
constexpr auto JsonStr()
{
    constexpr size_t LEN = Sv.size() + 2;
    std::array<char, LEN> arr{'"'};
    arr[LEN - 1] = '"';

    std::copy(Sv.begin(), Sv.end(), arr.begin() + 1);

    return arr;
}

// template <auto const& Sv>
// struct json_t
// {
//     static constexpr auto value = StrJson<Sv>();
//     static constexpr auto size() { return value.size(); }
// };

// object
// todO: put std::span instead of auto here maybe cuz std::array can be used to construct it
template <std::pair<std::string_view, auto> const&... p>
constexpr auto JsonObj()
{
    constexpr std::size_t extra =
        2 + sizeof...(p) * 4 - 1;  // two {} + colons + quotes for fields + comma
    constexpr std::size_t len =
        (p.first.size(), ...) + (p.second.size(), ...) + extra;

    std::array<char, len> arr{'{'};
    auto append = [i = 1, &arr](auto const& s) mutable
    {
        arr[i++] = '\"';

        std::copy(s.first.begin(), s.first.end(), arr.begin() + i);
        i += s.first.size();

        arr[i++] = '\"';
        arr[i++] = ':';

        std::copy(s.second.begin(), s.second.end(), arr.begin() + i);
        i += s.second.size();
        arr[i] = ',';
    };
    (append(p), ...);

    arr[len - 1] = '}';
    return arr;
}

struct json_obj_t
{
};

template <auto const&... Strs>
struct join
{
    // Join all strings into a single std::array of chars
    static constexpr auto impl() noexcept
    {
        constexpr std::size_t extra =
            2 + (sizeof...(Strs) - 1);  // two [] + number of commas
        constexpr std::size_t len = (Strs.size() + ... + 0) + extra;
        std::array<char, len + 1> arr{'['};
        auto append = [i = 1, &arr](auto const& s) mutable
        {
            // constexpr auto x = StrJson(s);
            // for (auto c : x) arr[i++] = c;
            for (auto c : s) arr[i++] = c;
            arr[i] = ',';
            i++;
        };
        (append(Strs), ...);
        arr[len - 1] = ']';
        return arr;
    }
    // Give the joined string static storage
    static constexpr auto arr = impl();
    // View as a std::string_view
    static constexpr std::string_view value{arr.data(), arr.size() - 1};
};

static constexpr std::string_view a = "abc";
void TEST()
{
    // constexpr auto x = json_t<a>::value;
    // constexpr auto y = join<json_t<a>::value>::value;
}

#endif