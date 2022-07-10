#include <gtest/gtest.h>

#include "payments/payment_manager.hpp"

TEST(Payment, CalculateRewardsProp)
{
    double total_effort = 5000;
    std::string chain = "GTEST";

    miner_map miners;
    miners["x"].round_effort_map[chain] = 1000;
    miners["y"].round_effort_map[chain] = 2000;
    miners["z"].round_effort_map[chain] = 2000;

    std::vector<RoundShare> shares;
    PaymentManager::GetRewardsProp(shares, chain, 10e8, miners, total_effort,
                                   0.01);

    for (const auto& share : shares){
        if(share.address == "x"){
            ASSERT_EQ(share.share, 0.2);
            ASSERT_EQ(share.reward, 198000000);
        }
        else
        {
            ASSERT_EQ(share.share, 0.4);
            ASSERT_EQ(share.reward, 396000000);
        }
    }
}