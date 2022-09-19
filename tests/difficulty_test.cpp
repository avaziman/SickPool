#include <gtest/gtest.h>

#include "crypto/utils.hpp"
#include "crypto/verushash/byteswap.h"
#include "crypto/verushash/uint256.h"

// TEST(DifficultyTest, Diff1BitsToFloat1)
// {
//     double diff = BitsToDiff(0x1f00ffff);
//     ASSERT_EQ(diff, 1.52587890625e-05);
// }

TEST(DifficultyTest, Diff1BitsToFloat)
{
    double diff = BitsToDiff(DIFF1_BITS);
    ASSERT_EQ(diff, 1.0);
}


TEST(DifficultyTest, RandomEqualityCheck)
{
    int randomDiff = abs(rand() * 1000);

    uint32_t bits = DiffToBits(randomDiff);
    double diff = BitsToDiff(bits);

    double ratio = randomDiff / diff;

    ASSERT_TRUE(ratio > 0.9 && ratio < 1.1);
}