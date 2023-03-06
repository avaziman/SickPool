#ifndef DAEMON_RESPONSES_BTC_HPP_
#define DAEMON_RESPONSES_BTC_HPP_

#include <vector>
#include <string_view>

struct TxRes
{
    std::string_view data;
    std::string_view hash;
    int64_t fee;
};

#endif