#ifndef ROUND_SHARE_HPP_
#define ROUND_SHARE_HPP_

#include <cstdint>
#include <string>
#include <vector>

typedef std::vector<std::pair<std::string, int64_t>> reward_map_t;

#pragma pack(push, 1)

struct RoundShare
{
    double effort;
    double share;
    int64_t reward;
};
#pragma pack(pop)
struct PaymentSettings
{
    int64_t threshold;
    // bool pool_block_only;
};

struct PayeeInfo
{
    int64_t amount;
    std::vector<uint8_t> script_pub_key;
    PaymentSettings settings;
};

struct PaymentTx{
    int64_t total_paid = 0;
    double fee = 0;
    reward_map_t rewards;
    std::string raw_transaction_hex;
    std::string tx_hash_hex;
};
typedef std::vector<std::pair<std::string, RoundShare>> round_shares_t;

#endif