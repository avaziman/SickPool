#include <fmt/format.h>
#include <gtest/gtest.h>
#include <hiredis/hiredis.h>

#include "block_submission.hpp"
#include "redis/redis_manager.hpp"

class RedisTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        rc = redisConnect("127.0.0.1", 6379);

        strcpy((char*)submission.miner, "RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC");
        strcpy((char*)submission.worker,
               "RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC.worker");
        strcpy((char*)submission.chain, "GTEST");
    }

    RedisManager redis_manager{"127.0.0.1", 6379};
    redisContext* rc;
    BlockSubmission submission
    {
        .confirmations = rand(), .blockReward = rand(), .timeMs = rand(),
        .durationMs = rand(), .height = (uint32_t)rand(),
        .number = (uint32_t)rand(), .difficulty = (double)rand(),
        .effortPercent = (double)rand()
    };
    // void TearDown() override {}
};

TEST_F(RedisTest, AddBlockSubmission)
{
    using namespace std::string_view_literals;

    std::string block_key = fmt::format("block:{}", submission.number);

    strcpy((char*)submission.miner, "RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC");
    strcpy((char*)submission.worker,
           "RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC.worker");
    strcpy((char*)submission.chain, "GTEST");

    bool res = redis_manager.AddBlockSubmission(&submission);

    ASSERT_TRUE(res);

    // make sure everything is in the database
    auto reply = (redisReply*)redisCommand(rc, "GET %s", block_key.c_str());

    BlockSubmission received = *((BlockSubmission*)reply->str);

    ASSERT_EQ(memcmp(&submission, &received, sizeof(BlockSubmission)), 0);

    // check one index exists, assume the rest do too as they use same func
    reply = (redisReply*)redisCommand(rc, "ZSCORE block-index:number %u",
                                      submission.number);
    ASSERT_STREQ(reply->str, std::to_string(submission.number).c_str());
}

TEST_F(RedisTest, UpdateBlockConfirmation)
{
    const int confirmations = 10;

    bool res = redis_manager.AddBlockSubmission(&submission);

    redis_manager.UpdateBlockConfirmations(std::to_string(submission.number), confirmations);
    auto reply = (redisReply*)redisCommand(rc, "GET block:%u", submission.number);
    BlockSubmission received = *((BlockSubmission*)reply->str);

    ASSERT_EQ(confirmations, received.confirmations);
}

TEST_F(RedisTest, SetMinerShares){
    std::vector<RoundShare> miner_shares{RoundShare{
        .address = "ADDRESS", .effort = 1.f, .share = 0.1, .reward = 1}};

    bool res = redis_manager.AddMinerShares("GTEST", &submission, miner_shares);

    ASSERT_TRUE(res);
}