#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "../src/crypto/hash_wrapper.hpp"
#include "../src/crypto/merkle_tree.hpp"
#include "../src/crypto/utils.hpp"

TEST(MerkleRootTest, SHA256MerkleRoot1Tx)
{
    HashWrapper::InitSHA256();
    // VRSC block #1:
    // 027e3758c3a65b12aa1046462b486d0a63bfa1beae327897f56c5cfb7daaae71
    char tx[] =
        "0100000001000000000000000000000000000000000000000000000000000000000000"
        "0000ffffffff03510101ffffffff01a0ab9200000000002321032718b2f8a8ea206db7"
        "a16428e99f7dcac1337a3a75c92631baee895f3239e5b6ac3072025b";

    const int txSize = (sizeof(tx) - 1);
    unsigned char txBytes[txSize / 2];
    Unhexlify(txBytes, tx, txSize);

    std::vector<unsigned char> txVec(txBytes, txBytes + txSize / 2);
    std::vector<std::vector<unsigned char>> txs = {txVec};

    unsigned char result[32];
    MerkleTree::CalcRoot(txs, result);

    char resultHex[64];
    Hexlify(resultHex, result, 32);

    char resVeresed[] =
        "ab9ef7fe49d1dda1ad66be7929d3ae18e284f89e64f4ae66f961c8ebada835a7";

    ASSERT_EQ(std::memcmp(resultHex, resVeresed, 64), 0);
}

TEST(MerkleRootTest, SHA256MerkleRoot2Tx)
{
    HashWrapper::InitSHA256();
    // VRSC block #1000000:
    // 473194b1e3301c0fac3b89ab100aedefb7c790aa9255f2e95be9da71bdb91050
    char tx1[] =
        "0400008085202f89010000000000000000000000000000000000000000000000000000"
        "000000000000ffffffff060340420f0101ffffffff0100180d8f00000000c32ea22c80"
        "20f17e0de592f8812d42cfe996da67a9ec4ee56ac667edfaba47e0fa0401b625c08103"
        "1210008203000401cc4c90040101010221022ec0fa3cb4d36a646138e02e91570b2424"
        "69f7ea325bb07205048d40183745a42103166b7813a4855a88e9ef7340a692ef3c2dec"
        "edfdc2c7563ec79537e89667d9352009d06c734c420d2fa63cac0c379629b4573cd555"
        "33629a51d2fe519d15d0fbe220ec77b5e38d43b195caf66f68d25ff38d886262f5d2fe"
        "bcb1c0cb207fdfc30ed90440420f0075727cb35e00000000000000000000000000000"
        "0";

    char tx2[] =
        "0400008085202f8901898fa27eb5d2d495e9269c11c9e138c853054ab7a644c3d85ac8"
        "49fd4d6cdfd7000000006a4730440220705bf400b43ae421b65667397baa96535f300c"
        "dc6a1a7d61351881c20826e17702206be03c46aa90098071c405e9cd7920867283769a"
        "3e9e2e0b226e576080227c690121022ec0fa3cb4d36a646138e02e91570b242469f7ea"
        "325bb07205048d40183745a4ffffffff0200b0eb0e0a0000001976a91445ce0a71d39a"
        "824181442a9979e44a38b900baeb88ac00000000000000004f6a4c4c5203b6ca0e0340"
        "420f20ec77b5e38d43b195caf66f68d25ff38d886262f5d2febcb1c0cb207fdfc30ed9"
        "21022ec0fa3cb4d36a646138e02e91570b242469f7ea325bb07205048d40183745a400"
        "000000a4420f000000000000000000000000";

    unsigned char txBytes1[sizeof(tx1) / 2];
    Unhexlify(txBytes1, tx1, sizeof(tx1));

    unsigned char txBytes2[sizeof(tx2) / 2];
    Unhexlify(txBytes2, tx2, sizeof(tx2));

    std::vector<unsigned char> txVec1(txBytes1, txBytes1 + sizeof(txBytes1));
    std::vector<unsigned char> txVec2(txBytes2, txBytes2 + sizeof(txBytes2));
    std::vector<std::vector<unsigned char>> txs = {txVec1, txVec2};

    unsigned char result[32];
    MerkleTree::CalcRoot(txs, result);

    char resultHex[64];
    Hexlify(resultHex, result, 32);

    char resVeresed[] =
        "b734860607317db2da7c040d6b94c92a4f0fdbad973a87134b9c4d64249834e8";

    ASSERT_EQ(std::memcmp(resultHex, resVeresed, 64), 0);
}