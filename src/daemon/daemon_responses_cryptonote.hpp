#ifndef DAEMON_RESPONSES_ZANO_HPP_
#define DAEMON_RESPONSES_ZANO_HPP_

#include <simdjson.h>

#include <string_view>
#include <vector>

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
struct BlockTemplateResCn
{
    simdjson::ondemand::document doc;

    std::string_view blob;
    uint64_t difficulty;
    uint32_t height;
    std::string_view prev_hash;
    std::string_view seed;
};

struct TransferResCn {
    std::string_view txid;
    std::size_t tx_size;
    // std::string_view txhex;
};

#endif