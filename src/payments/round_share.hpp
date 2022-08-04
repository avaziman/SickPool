#ifndef ROUND_SHARE_HPP_
#define ROUND_SHARE_HPP_

#include <cstdint>
#include <string>
#include <vector>

#pragma pack(push, 1)

struct RoundShare
{
    double effort;
    double share;
    int64_t reward;
};
#pragma pack(pop)

struct AgingBlock
{
    uint32_t id;
    uint8_t coinbase_txid[HASH_SIZE];
    int64_t matued_at_ms;
    int64_t reward;
};

struct PendingPayment
{
    int64_t amount;
    std::vector<uint8_t> script_pub_key;
};

struct PaymentSettings
{
    int64_t threshold;
    // bool pool_block_only;
};
struct RewardInfo
{
    int64_t reward;
    PaymentSettings settings;
};

typedef std::vector<std::pair<std::string, RoundShare>> round_shares_t;
typedef std::vector<std::pair<std::string, int64_t>> reward_map_t;

#endif