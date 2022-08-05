#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "../src/crypto/hash_wrapper.hpp"
#include "../src/crypto/merkle_tree.hpp"
#include "../src/crypto/utils.hpp"

using namespace std::string_literals;

TEST(MerkleRootTest, SHA256MerkleRoot1Tx)
{
    HashWrapper::InitSHA256();
    // VRSC block #1:
    // 000007b2d09d316ca1763b8922c769aabc898043274849ce64e147fa91d023cb
    TransactionData tx1(
        "a735a8adebc861f966aef4649ef884e218aed32979be66ada1ddd149fef79eab"s);

    std::vector<TransactionData> vec{tx1};

    uint8_t result[HASH_SIZE];
    MerkleTree::CalcRoot(result, vec);

    // merkle root should be same as hash of single tx
    ASSERT_EQ(std::memcmp(result, tx1.hash, HASH_SIZE), 0);
}

TEST(MerkleRootTest, SHA256MerkleRoot2Tx)
{
    HashWrapper::InitSHA256();
    // VRSC block #1000000:
    // 473194b1e3301c0fac3b89ab100aedefb7c790aa9255f2e95be9da71bdb91050
    TransactionData tx1(
        "baa658081ea730e0e591a29c914133d4370cb76365118ac7077c3a864b9235b7"s);

    TransactionData tx2(
        "7a9dee91d6faf67f18bea275db221b5d68f94e655710da1297dd8896d53176b3"s);

    const std::vector<TransactionData> txs{tx1, tx2};

    uint8_t result[HASH_SIZE];
    MerkleTree::CalcRoot(result, txs);
    std::reverse(result, result + sizeof(result));

    uint8_t expected[] = {0xe8, 0x34, 0x98, 0x24, 0x64, 0x4d, 0x9c, 0x4b,
                          0x13, 0x87, 0x3a, 0x97, 0xad, 0xdb, 0x0f, 0x4f,
                          0x2a, 0xc9, 0x94, 0x6b, 0x0d, 0x04, 0x7c, 0xda,
                          0xb2, 0x7d, 0x31, 0x07, 0x06, 0x86, 0x34, 0xb7};

    ASSERT_EQ(std::memcmp(result, expected, HASH_SIZE), 0);
}