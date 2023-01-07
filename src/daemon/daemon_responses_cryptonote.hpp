#ifndef DAEMON_RESPONSES_ZANO_HPP_
#define DAEMON_RESPONSES_ZANO_HPP_

#include <simdjson.h>

#include <string_view>
#include <vector>

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
    simdjson::ondemand::document doc;

    std::string_view txid;
    std::size_t tx_size;
};

struct BlockHeaderResCn
{
    simdjson::ondemand::document doc;

    uint32_t depth;
    std::string_view hash;
};

struct AliasRes  {
    simdjson::ondemand::document doc;
    std::string_view address;
};
#endif