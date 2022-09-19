#ifndef DAEMON_RESPONSES_SIN_HPP_
#define DAEMON_RESPONSES_SIN_HPP_

#include <vector>
#include <string_view>

struct OutputRes {
    std::string_view script_hex;
    int64_t value;
};

struct TxRes {
    std::string_view data;
    std::string_view hash;
    int64_t fee;
};

// in the order they appear, in the type they appear
struct BlockTemplateRes
{
    simdjson::ondemand::document doc;

    int32_t version;
    std::string_view prev_block_hash;
    std::vector<TxRes> transactions;
    // includes, inf nodes, dev fees, and tx fees
    int64_t coinbase_value;
    int64_t min_time;
    std::string_view bits;
    uint32_t height;
    std::vector<OutputRes> infinity_nodes;
    std::vector<OutputRes> dev_fee;
};

#endif