#ifndef ROUND_SHARE_HPP_
#define ROUND_SHARE_HPP_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "transaction.hpp"
#include "block_template.hpp"
#include "stats.hpp"

using reward_map_t = std::vector<std::pair<std::string, int64_t>>;

#pragma pack(push, 1)
struct RoundReward
{
    double effort;
    double share;
    int64_t reward;
};
#pragma pack(pop)

constexpr auto round_reward_size = sizeof(RoundReward);

// struct PaymentSettings
// {
//     bool pool_block_only;
// };

// struct PayeeInfo
// {
//     uint64_t amount;
//     PaymentSettings settings;
// };

struct Payee
{
    uint32_t miner_id;
    uint64_t amount_clean;  // substructed tx fee
    std::string address;
};

struct PayoutInfo
{
    std::string txid;
    uint64_t tx_fee;
    uint64_t time;
    uint64_t total;
    uint32_t id;
};

using round_shares_t = std::unordered_map<MinerId, RoundReward>;

#endif