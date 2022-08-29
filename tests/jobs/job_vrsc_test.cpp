#include "static_config.hpp"
#if COIN == VRSC
#include "job_vrsc.hpp"

#include <gtest/gtest.h>

#include "block_template.hpp"

TEST(Jobs, JobVrscTest)
{
    using namespace std::string_view_literals;
    // mainnet block 2000000

    BlockTemplateVrsc btemplate;
    btemplate.version = 65540;
    btemplate.prev_block_hash =
        "1664c73bac352ce0dd85866ca665f4f1890f65705f686bed5d799064810010aa";
    btemplate.coinbase_value = 1200000000;
    btemplate.min_time = 1650823041;
    btemplate.bits = bswap_32(0x1b083611);
    btemplate.height = 2000000;
    btemplate.finals_root_hash =
        "13650acb8a03e471368462165ae8f1b021bb2cb68d094cf652a16e7743388b32";
    btemplate.solution =
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000";

    // coinabse
    TransactionData td(
        "0400008085202f89010000000000000000000000000000000000000000000000000000"
        "000000000000ffffffff050380841e00ffffffff01008c8647000000001976a914edec"
        "07eae8f4db824f89bcccc564fff9910872cb88ac000000000000000000000000000000"
        "00000000",
        "8ec67eb75a81bad87a33314859ac543553881489fd55981dd2dbd6f13008c1d7");
    btemplate.tx_list.AddCoinbaseTxData(td);

    JobVrsc job("00000000", btemplate, false);

    ASSERT_EQ(
        job.GetNotifyMessage(),
        "{\"id\":null,\"method\":\"mining.notify\",\"params\":["
        "\"00000000\",\"04000100\","
        "\"aa1000816490795ded6b685f70650f89f1f465a66c8685dde02c35ac3bc76416\","
        "\"d7c10830f1d6dbd21d9855fd891488533554ac594831337ad8ba815ab77ec68e\","
        "\"328b3843776ea152f64c098db62cbb21b0f1e85a1662843671e4038acb0a6513\","
        "\"818f6562\",\"1b083611\",true,"
        "\"00000000000000000000000000000000000000000000000000000000000000000000"
        "00"
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000\"]}\n"sv);
}
#endif