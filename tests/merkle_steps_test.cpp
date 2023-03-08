#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "../src/crypto/hash_wrapper.hpp"
#include "../src/crypto/merkle_tree.hpp"
#include "../src/crypto/utils.hpp"

using namespace std::string_literals;

// return merkle steps as they are passed in the job notification
// from hex txids as from daemon (reversed block encoding)

constexpr auto HASHSIZE = 32;
using HashT = std::array<uint8_t, HASH_SIZE>;

std::vector<std::string> GetMerkleSteps(
    std::initializer_list<std::string_view> txids_hex)
{
    HashWrapper::InitSHA256();

    std::vector<HashT> txids_bin;

    // unhexlify and reverse to block encoding
    for (std::string_view txid_hex : txids_hex)
    {
        auto hash = Unhexlify<HASH_SIZE * 2>(txid_hex);
        std::ranges::reverse(hash);
        txids_bin.push_back(hash);
    }

    auto steps_bin = MerkleTree<HASHSIZE>::CalcSteps(txids_bin);

    std::vector<std::string> res_hex;
    for (const auto& step : steps_bin)
    {
        auto step_hex = Hexlify(step);

        res_hex.push_back(std::string(step_hex.begin(), step_hex.size()));
    }

    return res_hex;
}

// coinbase txid in correct byte order
// steps hexlified in correct byte order
std::string GetRootFromSteps(std::string cbtxid,
                             std::vector<std::string>& steps)
{
    std::vector<HashT> steps_bin;
    uint8_t cbtxid_bin[HASH_SIZE];

    for (const auto& step : steps)
    {
        auto step_bin = Unhexlify<HASHSIZE * 2>(step);
        steps_bin.push_back(step_bin);
    }

    Unhexlify(cbtxid_bin, cbtxid.data(), HASH_SIZE_HEX);

    uint8_t merkle_root_bin[HASH_SIZE];
    MerkleTree<HASHSIZE>::CalcRootFromSteps(merkle_root_bin, cbtxid_bin,
                                            steps_bin, steps.size());

    std::string merkle_root_hex;
    merkle_root_hex.resize(HASH_SIZE_HEX);
    Hexlify(merkle_root_hex.data(), merkle_root_bin, HASH_SIZE);
    return merkle_root_hex;
}

TEST(MerkleStepsTest, SHA256MerkleSteps1Tx)
{
    HashWrapper::InitSHA256();
    // VRSC block #1:
    // 000007b2d09d316ca1763b8922c769aabc898043274849ce64e147fa91d023cb
    auto merkle_steps = GetMerkleSteps(
        {"a735a8adebc861f966aef4649ef884e218aed32979be66ada1ddd14"
         "9fef79eab"});

    // no merkle steps for 1 tx
    ASSERT_TRUE(merkle_steps.empty());

    auto root = GetRootFromSteps(
        "ab9ef7fe49d1dda1ad66be7929d3ae18e284f89e64f4ae66f961c8ebada835a7",
        merkle_steps);

    ASSERT_EQ(
        root,
        "ab9ef7fe49d1dda1ad66be7929d3ae18e284f89e64f4ae66f961c8ebada835a7");
}

TEST(MerkleStepsTest, SHA256MerkleSteps2Tx)
{
    HashWrapper::InitSHA256();
    // VRSC block #1000000:
    // 473194b1e3301c0fac3b89ab100aedefb7c790aa9255f2e95be9da71bdb91050
    auto merkle_steps = GetMerkleSteps(
        {"baa658081ea730e0e591a29c914133d4370cb76365118ac7077c3a864b9235b7",
         "7a9dee91d6faf67f18bea275db221b5d68f94e655710da1297dd889"
         "6d53176b3"});

    ASSERT_EQ(merkle_steps.size(), 1);
    ASSERT_EQ(
        merkle_steps[0],
        "b37631d59688dd9712da1057654ef9685d1b22db75a2be187ff6fad691ee9d7a");

    auto root = GetRootFromSteps(
        "b735924b863a7c07c78a116563b70c37d43341919ca291e5e030a71e0858a6ba",
        merkle_steps);

    ASSERT_EQ(
        root,
        "b734860607317db2da7c040d6b94c92a4f0fdbad973a87134b9c4d64249834e8");
}

TEST(MerkleStepsTest, SHA256MerkleSteps3Tx)
{
    HashWrapper::InitSHA256();
    // SIN block #1133439:
    // 0fbfb95728776106d08815ba67786240c0d3fa31e6d32d3ea786b265a42a5365
    auto merkle_steps = GetMerkleSteps(
        {"b0175192e717aab4358d058cbf138525c6ce19ccaa98a31bcd88969704baed93",
         "225e84b7ed0444cbd3df775b1cb01c1bbb1fa79efa769b9abe212a4ff6c13084",
         "bd71c318d71af9282bf2dec676bcdeef0b5aaeadf2a31d5479829ea344f182f4",
         "bd71c318d71af9282bf2dec676bcdeef0b5aaeadf2a31d5479829ea344f182f4"});
    ASSERT_EQ(
        merkle_steps,
        std::vector<std::string>(
            {"8430c1f64f2a21be9a9b76fa9ea71fbb1b1cb01c5b77dfd3cb4404edb7845e22",
             "8a2903a733a8b094d236ac04c322cbc187bdc26c6ac320cbb6fbe35991eaf81"
             "9"}));

    auto root = GetRootFromSteps(
        "93edba04979688cd1ba398aacc19cec6258513bf8c058d35b4aa17e7925117b0",
        merkle_steps);

    ASSERT_EQ(
        root,
        "4b19ebc366165caa0f2330fbbcde06ad8cfd1ace260193cefbf7f998aa51d4cf");
}

TEST(MerkleStepsTest, SHA256MerkleSteps4Tx)
{
    HashWrapper::InitSHA256();
    // SIN block #1133696:
    // 9839d6e059d41a928ffa23843e7125c68561c60fcd13e2257764ed58440672c4

    auto merkle_steps = GetMerkleSteps({
        "c1d1ae759572c9792338b12aaaa12548e136b4c5aeefb145bba8953925caf3e5",
        "359ed65c6377fc8a425d29acdd2b5e567b0bdaee0e003c7ed5df588a3346b3a0",
        "96b608b4b89aea059473c1eeef85a27b087cc660b166952bca7553e93ebbd664",
        "9dbc3445b2f5917976c7aa2aa54b7def2548e4dd8cfee86495ff5226e36d7c71",
    });

    ASSERT_EQ(
        merkle_steps,
        std::vector<std::string>(
            {"a0b346338a58dfd57e3c000eeeda0b7b565e2bddac295d428afc77635cd69e35",
             "5fa3a8bfaf3cc7e9d15e1c63ff07f8837b882e3b09e915b507a6a9a31246a22"
             "c"}));

    auto root = GetRootFromSteps(
        "e5f3ca253995a8bb45b1efaec5b436e14825a1aa2ab1382379c9729575aed1c1",
        merkle_steps);

    ASSERT_EQ(
        root,
        "f01b9e318508b61c335bd856efb27ad7826fc8363878e95e45a9c6f361fbfd03");
}
