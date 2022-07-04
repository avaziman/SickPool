// #include <gtest/gtest.h>

// #include "crypto/utils.hpp"
// #include "crypto/verushash/byteswap.h"
// #include "crypto/verushash/uint256.h"

// TEST(DifficultyTest, Diff1BitsToFloat)
// {
//     double diff = BitsToDiff(DIFF1_BITS);
//     ASSERT_EQ(diff, 1.0);
// }

// TEST(DifficultyTest, RandomEqualityCheck)
// {
//     int randomDiff = rand() * 1000;

//     uint32_t bits = DiffToBits(randomDiff);
//     double diff = BitsToDiff(bits);

//     double ratio = randomDiff / diff;

//     ASSERT_TRUE(ratio > 0.9 && ratio < 1.1);
// }

// TEST(DifficultyTest, BlocksTargetTest)
// {
//     for (int i = 0; i < sizeof(hashes); i++)
//     {
//         uint32_t hash_bits = UintToArith256(uint256S(hashes[i])).GetCompact();
//         double hash_diff = BitsToDiff(hash_bits);
//         double min_diff = BitsToDiff(bits[i]);

//         std::cout << i << " hash: " << hashes[i] << std::hex << " min bits "
//                   << bits[i] << " hash bits " << hash_bits << std::dec << " "
//                   << min_diff << " < " << hash_diff << std::endl;
//         ASSERT_TRUE(hash_diff > min_diff);
//     }
// }