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
    std::vector<uint8_t> tx1 = {0xa7, 0x35, 0xa8, 0xad, 0xeb, 0xc8, 0x61, 0xf9,
                                0x66, 0xae, 0xf4, 0x64, 0x9e, 0xf8, 0x84, 0xe2,
                                0x18, 0xae, 0xd3, 0x29, 0x79, 0xbe, 0x66, 0xad,
                                0xa1, 0xdd, 0xd1, 0x49, 0xfe, 0xf7, 0x9e, 0xab};

    uint8_t result[HASH_SIZE];
    MerkleTree::CalcRoot(result, tx1, 1);

    // merkle root should be same as hash of single tx
    ASSERT_EQ(std::memcmp(result, tx1.data(), HASH_SIZE), 0);
}

TEST(MerkleRootTest, SHA256MerkleRoot2Tx)
{
    HashWrapper::InitSHA256();
    // VRSC block #1000000:
    // 473194b1e3301c0fac3b89ab100aedefb7c790aa9255f2e95be9da71bdb91050
    auto tx1 =
        "b735924b863a7c07c78a116563b70c37d43341919ca291e5e030a71e0858a6ba"s;

    auto tx2 =
        "b37631d59688dd9712da1057654ef9685d1b22db75a2be187ff6fad691ee9d7a"s;

    std::vector<uint8_t> txs(tx1.size() + tx2.size());
    Unhexlify(txs.data(), tx1.data(), tx1.size());
    Unhexlify(txs.data() + HASH_SIZE, tx2.data(), tx2.size());

    uint8_t result[HASH_SIZE];
    MerkleTree::CalcRoot(result, txs, 2);
    std::reverse(result, result + sizeof(result));

    uint8_t expected[] = {0xe8, 0x34, 0x98, 0x24, 0x64, 0x4d, 0x9c, 0x4b,
                          0x13, 0x87, 0x3a, 0x97, 0xad, 0xdb, 0x0f, 0x4f,
                          0x2a, 0xc9, 0x94, 0x6b, 0x0d, 0x04, 0x7c, 0xda,
                          0xb2, 0x7d, 0x31, 0x07, 0x06, 0x86, 0x34, 0xb7};

    ASSERT_EQ(std::memcmp(result, expected, HASH_SIZE), 0);
}