// #include <gtest/gtest.h>

// #include <cstring>
// #include <vector>

// #include "../src/crypto/hash_wrapper.hpp"
// #include "../src/crypto/merkle_tree.hpp"
// #include "../src/crypto/utils.hpp"

// using namespace std::string_literals;

// // return merkle root as in daemon encoding (reverse block encoding)
// // from hex txids as from daemon (reversed block encoding)
// std::string GetMerkleRoot(std::initializer_list<std::string_view> txids_hex)
// {
//     HashWrapper::InitSHA256();

//     std::vector<uint8_t> txids_bin(txids_hex.size() * HASH_SIZE);

//     int i = 0;
//     // unhexlify and reverse to block encoding
//     for (std::string_view txid_hex : txids_hex)
//     {
//         Unhexlify(txids_bin.data() + i * HASH_SIZE, txid_hex.data(),
//                   HASH_SIZE_HEX);
//         std::reverse(txids_bin.data() + i * HASH_SIZE,
//                      txids_bin.data() + (i + 1) * HASH_SIZE);
//         i++;
//     }

//     std::string result_hex;
//     result_hex.resize(HASH_SIZE_HEX);
//     uint8_t result[HASH_SIZE];
//     MerkleTree::CalcRoot(result, txids_bin, txids_hex.size());

//     // reverse and hexlify as in daemon encoding
//     std::reverse(result, result + HASH_SIZE);
//     Hexlify(result_hex.data(), result, HASH_SIZE);
//     return result_hex;
// }

// TEST(MerkleRootTest, SHA256MerkleRoot1Tx)
// {
//     HashWrapper::InitSHA256();
//     // VRSC block #1:
//     // 000007b2d09d316ca1763b8922c769aabc898043274849ce64e147fa91d023cb
//     auto merkle_root =
//         GetMerkleRoot({"a735a8adebc861f966aef4649ef884e218aed32979be66ada1ddd14"
//                        "9fef79eab"});

//     // merkle root should be same as hash of single tx
//     ASSERT_TRUE(
//         merkle_root ==
//         "a735a8adebc861f966aef4649ef884e218aed32979be66ada1ddd149fef79eab");
// }

// TEST(MerkleRootTest, SHA256MerkleRoot2Tx)
// {
//     HashWrapper::InitSHA256();
//     // VRSC block #1000000:
//     // 473194b1e3301c0fac3b89ab100aedefb7c790aa9255f2e95be9da71bdb91050
//     auto merkle_root =
//         GetMerkleRoot({"baa658081ea730e0e591a29c914133d4370cb76365118ac7077c3a8"
//                        "64b9235b7",
//                        "7a9dee91d6faf67f18bea275db221b5d68f94e655710da1297dd889"
//                        "6d53176b3"});

//     ASSERT_TRUE(
//         merkle_root ==
//         "e8349824644d9c4b13873a97addb0f4f2ac9946b0d047cdab27d3107068634b7");
// }

// TEST(MerkleRootTest, SHA256MerkleRoot10Tx)
// {
//     HashWrapper::InitSHA256();
//     // SIN block #1132447:
//     // 473194b1e3301c0fac3b89ab100aedefb7c790aa9255f2e95be9da71bdb91050

//     auto merkle_root = GetMerkleRoot(
//         {"cab263ee398707e220bc35f4b37aa78a55d05c9b86f429cf67dd0d35ae0b1162",
//          "6818a83d69e954a54294d7e112653fad588d0e053a0d2bc7eea706d2473b2739",
//          "05b3e3cc0794459df506ff6482e752ebd8de31b1c767984c06543b7bf4a6ed46",
//          "307b448407d81c4b6c9dd36fc306bb8b88b6aabed25c9da6957185d2ab7311f8",
//          "f18177bb3a76658741a87456c4a861a548bdf42a5facbc4a9aa010a2b778c22a",
//          "8297ddb13ff575eb5b643183eac974bbbb688a78323b8de492bc94d3bf765506",
//          "17cc622662b544bb95cea9edc7eee75473f35d8668a31f63dc101ecebe6b5a4d",
//          "336d57a9c20f3aebd2999ad68d7434725a5e8079ddf18ecc5bbe681f24942068",
//          "eb1790cd43d5445b6274d6d84f310a373351135e0de285c26d49a6979d518e51",
//          "f49f2b0d6bfe610ca5a7c174c07be17eb7c5bda2ff63a32e9b415a4382b7d6ef",
//          "e9c8eae04f562585c7acb367d3d20acad13f766a75d7b6a1fd2bf7a4d23f4cde",
//          "4c7ce975c7c40ec06ce9fb893349ae81ba1e21e945dd44e41b1b379c7fa144ae"
//         });

//     ASSERT_TRUE(
//         merkle_root ==
//         "3b251bf71d07790d31b057ce26c7daae3cbe30bf0fda8b4199292c81464b325c");
// }

// TEST(MerkleRootTest, SHA256MerkleRoot6Tx)
// {
//     HashWrapper::InitSHA256();
//     // SIN block #1132522:
//     // 473194b1e3301c0fac3b89ab100aedefb7c790aa9255f2e95be9da71bdb91050

//     auto merkle_root = GetMerkleRoot(
//         {"0b00e7279d3bb1adf3ff42d8f3903b9357e8f3bb1401a62726c89cbc12924060",
//          "00d433baaf8ed6fc2a0c380752430fd4d6d0f2a4395087950e1a46d61dc0ffe0",
//          "72c9f81a0ebeea8d8f0a1d805ec8a9fdbb7e26c21f1b0a7c8d3fc6526f324b7d",
//          "7a8d7b15d3731d2369441986702921d557c9b1e171b7f53aaac3e0185d4c6c0f",
//          "a2ac25455a26d2f2253d243d88fda321715e78908a775bbaa6de5e8723/track/7wCmS9TTVUcIhRalDYFgPye47931",
//          "576fa9a08c61e717fc10ca2abb567c815a5665fbe3c9686a5052433611a28018"});

//     ASSERT_TRUE(
//         merkle_root ==
//         "96113d098fc3c1d55dc231e1295f9672f9659018ad43d8c2fd26c2cc236436cf");
// }

// TEST(MerkleRootTest, SHA256MerkleRoot5Tx)
// {
//     HashWrapper::InitSHA256();
//     // SIN block #1132522:
//     // 473194b1e3301c0fac3b89ab100aedefb7c790aa9255f2e95be9da71bdb91050

//     auto merkle_root = GetMerkleRoot(
//         {"4fc2fe1a5d3bc91f4b5c5879342d45bd91cf4988e9c3cb6f4c7e235a72acc34b",
//          "be9401c8f09c1f6e392c00f909c2956251c921350dac093f21120ea8828041cf",
//          "59562758d776a23b93a2f826df816e54f97c6d7a41c96cb95d9e90887bae3377",
//          "c41e3fafda9dc60d37f252f5929fbb1dcc563c56366072b62615b36a6b55ad6e",
//          "38f5efd89257d6701fcbde6995a9e38178ce0639bc09a9209286e177818ba0af"});

//     ASSERT_TRUE(
//         merkle_root ==
//         "2515bff8175dc54c56c864c3e7de091bf3a99d93c27ec3fc668769e620bd5374");
// }