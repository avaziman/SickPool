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
    PaymentManager::GetRewardsProp(shares, 10e8, miners, total_effort,
                                   0.01);

    for (const auto& share : shares){
        if(share.first == "x"){
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