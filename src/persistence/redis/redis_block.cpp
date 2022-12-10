#ifndef REDIS_BLOCK_HPP
#define REDIS_BLOCK_HPP

#include "redis_block.hpp"

using enum Prefix;

// void RedisBlock::AppendAddBlockSubmission(const BlockSubmission& submission)
// {
//     using namespace std::string_view_literals;

//     uint32_t block_id = submission.number;
//     auto chain = submission.chain;
//     std::string block_id_str = std::to_string(block_id);
//     std::string_view block_id_sv(block_id_str);

//     AppendCommand({"INCR"sv, block_key_names.block_number});

//     {
//         RedisTransaction add_block_tx(this);

//         // serialize the block submission to save space and net
//         // bandwidth, as the indexes are added manually anyway no need for
//         // hash
//         AppendCommand(
//             {"SET"sv, Format({ block_key_names.block, std::to_string(block_id)}),
//              std::string_view(reinterpret_cast<const char*>(&submission), sizeof(BlockSubmission))});
//         /* sortable indexes */
//         // block no. and block time will always be same order
//         // so only one index is required to sort by either of them
//         // (block num value is smaller)
//         AppendCommand({"ZADD"sv, block_key_names.block_index_number,
//                        std::to_string(submission.number), block_id_sv});

//         AppendCommand({"ZADD"sv, block_key_names.block_index_reward,
//                        std::to_string(submission.reward), block_id_sv});

//         AppendCommand({"ZADD"sv, block_key_names.block_index_difficulty,
//                        std::to_string(submission.difficulty), block_id_sv});

//         AppendCommand({"ZADD"sv, block_key_names.block_index_effort,
//                        std::to_string(submission.effort_percent),
//                        block_id_sv});

//         AppendCommand({"ZADD"sv, block_key_names.block_index_duration,
//                        std::to_string(submission.duration_ms), block_id_sv});

//         /* non-sortable indexes */
//         AppendCommand({"SADD"sv,
//                        Format({block_key_names.block_index_chain,
//                                    std::to_string(submission.chain)}),
//                        block_id_sv});


//         AppendCommand(
//             {"SADD"sv,
//              Format({block_key_names.block_index_solver, std::to_string(submission.miner_id)}),
//              block_id_sv});

//         // AppendCommand(
//         //     {"SADD"sv, PrefixKey<BLOCK, INDEX, TYPE, POW>(), block_id_sv});
//         /* other block statistics */
//         // AppendTsAdd(PrefixKey<BLOCK, STATS, EFFORT_PERCENT>(),
//         //             submission.time_ms, submission.effort_percent);

//         // block number is written on interval.
//         // AppendTsAdd(PrefixKey<BLOCK, STATS, DURATION>(), submission.time_ms,
//         //             submission.duration_ms);

//     }
// }

bool RedisBlock::UpdateBlockConfirmations(std::string_view block_id,
                                          int32_t confirmations)
{
    using namespace std::string_view_literals;
    std::scoped_lock lock(rc_mutex);
    // redis bitfield uses be so gotta swap em
    return Command({
               "BITFIELD"sv, Format({block_key_names.block, block_id}), "SET"sv,
                   "i32"sv, "0"sv, std::to_string(bswap_32(confirmations))
           }).get() == nullptr;
}

bool RedisBlock::LoadImmatureBlocks(
    std::vector<std::unique_ptr<BlockSubmission>> &submissions)

{
    // auto reply =
    //     Command({"KEYS", fmt::format("{}*", block_key_names.reward_immature)});

    // for (int i = 0; i < reply->elements; i++)
    // {
    //     std::string_view block_id(reply->element[i]->str,
    //                               reply->element[i]->len);
    //     block_id =
    //         block_id.substr(block_id.find_last_of(":") + 1, block_id.size());

    //     auto block_reply = Command(
    //         {"GET", fmt::format("{}:{}", block_key_names.block, block_id)});

    //     if (block_reply->type != REDIS_REPLY_STRING)
    //     {
    //         continue;
    //     }

    //     auto submission = (BlockSubmission *)block_reply->str;
    //     auto extended =
    //         std::make_unique<BlockSubmission>(*submission);

    //     submissions.emplace_back(std::move(extended));
    // }

    return true;
}

uint32_t RedisBlock::GetBlockNumber()
{
    return GetInt(block_key_names.block_number);
};

bool RedisBlock::UpdateBlockNumber(int64_t time, uint32_t number)
{
    std::scoped_lock lock(rc_mutex);
    AppendTsAdd(block_key_names.block_number, time, number);
    return GetReplies();
}

bool RedisBlock::UpdateImmatureRewards(uint8_t chain, uint32_t block_num,
                                         int64_t matured_time, bool matured)
{
    using namespace std::string_view_literals;
    using namespace std::string_literals;
    std::scoped_lock lock(rc_mutex);

    // auto reply = Command({"HGETALL"sv, Format({block_key_names.reward_immature,
    //                                            std::to_string(block_num)})});

    // int64_t matured_funds = 0;
    // // either mature everything or nothing
    // {
    //     RedisTransaction update_rewards_tx(this);

    //     for (int i = 0; i < reply->elements; i += 2)
    //     {
    //         std::string_view addr(reply->element[i]->str,
    //                               reply->element[i]->len);

    //         RoundReward *miner_share =
    //             (RoundReward *)reply->element[i + 1]->str;

    //         // if a block has been orphaned only remove the immature
    //         // if there are sub chains add chain:

    //         if (!matured)
    //         {
    //             miner_share->reward = 0;
    //         }

    //         // used to show round share statistics
    //         uint8_t
    //             round_share_block_num[sizeof(miner_share) + sizeof(block_num)];
    //         memcpy(round_share_block_num, miner_share, sizeof(miner_share));
    //         memcpy(round_share_block_num + sizeof(miner_share), &block_num,
    //                sizeof(block_num));

    //         std::string mature_rewards_key =
    //             Format({block_key_names.reward_mature, addr});
    //         AppendCommand({"LPUSH"sv, mature_rewards_key,
    //                        std::string_view((char *)round_share_block_num,
    //                                         sizeof(round_share_block_num))});

    //         AppendCommand({"LTRIM"sv, mature_rewards_key, "0"sv,
    //                        std::to_string(ROUND_SHARES_LIMIT)});

    //         std::string reward_str = std::to_string(miner_share->reward);
    //         std::string_view reward_sv(reward_str);

    //         // // used for payments
    //         // AppendCommand({"HSET"sv, fmt::format("mature-rewards:{}", addr),
    //         //                std::to_string(block_num), reward_sv});

    //         AppendCommand(
    //             {"ZINCRBY"sv, block_key_names.solver_index_mature, reward_sv, addr});

    //         std::string solver_key = Format({block_key_names.solver, addr});

    //         AppendCommand({"HINCRBY"sv, solver_key, EnumName<MATURE_BALANCE>(),
    //                        reward_sv});
    //         AppendCommand({"HINCRBY"sv, solver_key,
    //                        EnumName<IMMATURE_BALANCE>(),
    //                        fmt::format("-{}", reward_sv)});

    //         // for payment manager...
    //         AppendCommand({"PUBLISH", block_key_names.block_mature_channel, "OK"});

    //         matured_funds += miner_share->reward;
    //     }

    //     // we pushed it to mature round shares list
    //     AppendCommand({"UNLINK"sv, Format({block_key_names.reward_immature,
    //                                        std::to_string(block_num)})});
    // }

    // logger.Log<LogType::Info>("{} funds have matured!", matured_funds);
    return GetReplies();
}

bool RedisBlock::SubscribeToMaturityChannel()
{
    Command({"SUBSCRIBE", block_key_names.block_mature_channel});
    return true;
}

#endif
