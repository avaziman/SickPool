#ifndef ROUND_SHARE_HPP_
#define ROUND_SHARE_HPP_

#include <cstdint>
#include <string>
#include <vector>
#include "verus_transaction.hpp"

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

struct PaymentInfo{
    int64_t total_paid = 0;
    int64_t fee = 0;
    reward_map_t rewards;
    std::string raw_transaction_hex;
    std::string tx_hash_hex;
};

struct PendingPayment
{
    PendingPayment(uint32_t id) : tx(TXVERSION, 0, true, TXVERSION_GROUP), id(id) {}
    uint32_t id;
    VerusTransaction tx;
    PaymentInfo info;
};

#pragma pack(push, 1)
struct FinishedPayment
{
    uint32_t id;
    char hash_hex[HASH_SIZE_HEX];
    int64_t total_paid_amount;
    int64_t time_ms;
    int64_t fee;
    uint32_t total_payees;
};

struct UserPayment
{
    uint32_t id;
    char hash_hex[HASH_SIZE_HEX];
    int64_t amount;
    int64_t time;
};
#pragma pack(pop)


typedef std::vector<std::pair<std::string, RoundShare>> round_shares_t;

#endif