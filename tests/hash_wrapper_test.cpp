#include "crypto/hash_wrapper.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(HashWrapperTest, VerusHash2_2)
{
    std::string hash = HashWrapper::VerushashV2b2(
        "Test1234Test1234Test1234Test1234Test1234Test1234Test1234Test1234Test12"
        "34Test1234Test1234Test1234");

    ASSERT_EQ(
        ReverseHex(hash),
        "ee2a1613d828cc0a39ba977dd20edd33ed30d7e41bbe2697e0912679de6aa830");
}