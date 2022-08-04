#include <gtest/gtest.h>

#include "payments/payment_manager.hpp"

TEST(Payment, CalculateRewardsProp)
{
    double total_effort = 5000;
    std::string chain = "GTEST";

    efforts_map_t miners;
    miners.emplace("x", 1000);
    miners.emplace("y", 2000);
    miners.emplace("z", 2000);

    std::vector<std::pair<std::string, RoundShare>> shares;
    PaymentManager::GetRewardsProp(shares, 10e8, miners, total_effort, 0.01);
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

// TEST(Payment, GenerateTx)
// {
//     PaymentManager payment_manager(0, 0);

//     AgingBlock aged_block;
//     aged_block.id = 4;
//     aged_block.matued_at_ms = 0;
//     memset(aged_block.coinbase_txid, 17, sizeof(aged_block.coinbase_txid));

//     reward_map_t rewards = {{"RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC", 1e8}};
//     payment_manager.AppendAgedRewards(aged_block, rewards);

//     std::vector<uint8_t> bytes;
//     payment_manager.tx.GetBytes(bytes);

//     char hex[bytes.size() * 2 + 1] = {0};
//     Hexlify(hex, bytes.data(), bytes.size());

//     // verus -chain=VRSCTEST createrawtransaction
//     // "[{\"txid\":\"1111111111111111111111111111111111111111111111111111111111111111\",\"vout\":0}]" 0 0
//     // "{\"RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC\":1}"
//     //  locktime 0 expiryheight 0

//     const char* res =
//         "0400008085202f89011111111111111111111111111111111111111111111111111111"
//         "1111111111110000000000ffffffff0100e1f505000000001976a914bf48c573ba8343"
//         "f061d43c3e3235ec7c6289df5488ac00000000000000000000000000000000000000";

//     ASSERT_STREQ(hex, res);
// }