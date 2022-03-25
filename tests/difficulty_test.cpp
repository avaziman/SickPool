#include <gtest/gtest.h>
#include "crypto/utils.hpp"
#include "crypto/verushash/byteswap.h"

TEST(DifficultyTest, Diff1BitsToFloat)
{
    double diff = BitsToDiff(DIFF1_BITS);
    ASSERT_EQ(diff, 1.0);
}

// TEST(DifficultyTest, Diff1FloatToBits)
// {
//     uint32_t bits = DiffToBits(1.0);
//     ASSERT_EQ(, 1);
// }

TEST(DifficultyTest, RandomEqualityCheck)
{
    int randomDiff = rand() * 1000;

    uint32_t bits = DiffToBits(randomDiff);
    double diff = BitsToDiff(bits);

    double ratio = randomDiff / diff;

    ASSERT_TRUE(ratio > 0.9 && ratio < 1.1);
}