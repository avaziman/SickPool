#include <gtest/gtest.h>

#include <span>

#include "payouts/payout_manager.hpp"
#include "round_share.hpp"
#include "test_utils.hpp"

TEST(Payment, CalculateRewardsProp)
{
    double total_effort = 5000;
    std::string chain = "GTEST";

    efforts_map_t miners;
    miners.emplace("x", 1000);
    miners.emplace("y", 2000);
    miners.emplace("z", 2000);

    round_shares_t shares;
    PaymentManager::GetRewardsPROP(shares, 10e8, miners, total_effort, 0.01);
    ASSERT_EQ(shares.size(), miners.size());

    for (const auto& share : shares)
    {
        if (share.first == "x")
        {
            ASSERT_EQ(share.second.share, 0.2);
            ASSERT_EQ(share.second.reward, 198000000);
        }
        else
        {
            ASSERT_EQ(share.second.share, 0.4);
            ASSERT_EQ(share.second.reward, 396000000);
        }
    }
}

TEST(Payment, CalculateRewardsPPLNS)
{
    double total_effort = 5000;
    std::string chain = "GTEST";

    double progress = 3.0;
    std::vector<Share> shares = {Share{{std::byte{2}}, 1.5},
                                 Share{{std::byte{3}}, progress}};

    const double n = 2;
    const int64_t block_reward = 100'000'000;
    round_shares_t rewards;
    const double fee = 0;
    PaymentManager::GetRewardsPPLNS(rewards, std::span<Share>(shares),
                                    block_reward, fee, n);
    // two shares:
    // '\3' -> 1.5 -> block reward * 0.75
    // '\2' -> 0.5 -> block_reward * 0.25

    ASSERT_EQ(rewards.size(), 2);
    auto rew1 = RoundShare{1, 0.75, static_cast<int64_t>(0.75 * block_reward)};
    auto rew2 = RoundShare{1, 0.25, static_cast<int64_t>(0.25 * block_reward)};
    ASSERT_FALSE(memcmp(&rewards["\3"], &rew1, sizeof(RoundShare)));
    ASSERT_FALSE(memcmp(&rewards["\2"], &rew2, sizeof(RoundShare)));

    // try with fee
    rewards.clear();
    const double fee1 = 0.01;
    PaymentManager::GetRewardsPPLNS(rewards, std::span<Share>(shares),
                                    block_reward, fee, n);
    ASSERT_EQ(rewards.size(), 2);
    auto rew11 = RoundShare{
        1, 0.75, static_cast<int64_t>((1 - fee1) * 0.75 * block_reward)};
    auto rew22 = RoundShare{
        1, 0.25, static_cast<int64_t>((1 - fee1) * 0.25 * block_reward)};
    ASSERT_FALSE(memcmp(&rewards["\3"], &rew1, sizeof(RoundShare)));
    ASSERT_FALSE(memcmp(&rewards["\2"], &rew2, sizeof(RoundShare)));

    // ASSERT_EQ(rewards["\2"], RoundShare{1, 0.25, 0.25 * block_reward});
}

// TEST(Payment, GenerateTx)
// {
//     PaymentManager payout_manager(0, 0);

//     AgingBlock aged_block;
//     aged_block.id = 4;
//     aged_block.matued_at_ms = 0;
//     memset(aged_block.coinbase_txid, 17, sizeof(aged_block.coinbase_txid));

//     reward_map_t rewards = {{"RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC", 1e8}};
//     payout_manager.AppendAgedRewards(aged_block, rewards);

//     std::vector<uint8_t> bytes;
//     payout_manager.tx.GetBytes(bytes);

//     char hex[bytes.size() * 2 + 1] = {0};
//     Hexlify(hex, bytes.data(), bytes.size());

//     // verus -chain=VRSCTEST createrawtransaction
//     //
//     "[{\"txid\":\"1111111111111111111111111111111111111111111111111111111111111111\",\"vout\":0}]"
//     0 0
//     // "{\"RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC\":1}"
//     //  locktime 0 expiryheight 0

//     const char* res =
//         "0400008085202f89011111111111111111111111111111111111111111111111111111"
//         "1111111111110000000000ffffffff0100e1f505000000001976a914bf48c573ba8343"
//         "f061d43c3e3235ec7c6289df5488ac00000000000000000000000000000000000000";

//     ASSERT_STREQ(hex, res);
// }