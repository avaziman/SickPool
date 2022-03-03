#ifndef SHARE_HPP_
#define SHARE_HPP_
#include <string_view>
struct Share{
    std::string_view worker;
    std::string_view jobId;
    std::string_view time;
    std::string_view nonce2;
    std::string_view solution;
};

#endif