#ifndef REDIS_BLOCK_HPP
#define REDIS_BLOCK_HPP

#include "redis_manager.hpp"

void RedisManager::AppendAddBlockSubmission(
    const ExtendedSubmission *submission)
{
    using namespace std::string_view_literals;

    uint32_t block_id = submission->number;
    auto chain = std::string(submission->chain_sv);
    std::string block_id_str = std::to_string(block_id);
    std::string_view block_id_sv(block_id_str);

    {
        RedisTransaction add_block_tx(this);

        // serialize the block submission to save space and net
        // bandwidth, as the indexes are added manually anyway no need for
        // hash
        AppendCommand(
            {"SET"sv, fmt::format("block:{}", block_id),
             std::string_view((char *)submission, sizeof(BlockSubmission))});
        /* sortable indexes */
        // block no. and block time will always be same order
        // so only one index is required to sort by either of them
        // (block num value is smaller)
        AppendCommand({"ZADD"sv, "block-index:number",
                       std::to_string(submission->number), block_id_sv});

        AppendCommand({"ZADD"sv, "block-index:reward",
                       std::to_string(submission->block_reward), block_id_sv});

        AppendCommand({"ZADD"sv, "block-index:difficulty",
                       std::to_string(submission->difficulty), block_id_sv});

        AppendCommand({"ZADD"sv, "block-index:effort",
                       std::to_string(submission->effort_percent),
                       block_id_sv});

        AppendCommand({"ZADD"sv, "block-index:duration",
                       std::to_string(submission->duration_ms), block_id_sv});

        /* non-sortable indexes */
        AppendCommand(
            {"SADD"sv,
             fmt::format("block-index:chain:{}", submission->chain_sv),
             block_id_sv});

        AppendCommand({"SADD"sv, "block-index:type:PoW", block_id_sv});

        AppendCommand(
            {"SADD"sv,
             fmt::format("block-index:solver:{}", submission->miner_sv),
             block_id_sv});

        AppendTsAdd(chain + ":round_effort_percent", submission->time_ms,
                    submission->effort_percent);
    }
}

bool RedisManager::UpdateBlockConfirmations(std::string_view block_id,
                                            int32_t confirmations)
{
    using namespace std::string_view_literals;
    std::scoped_lock lock(rc_mutex);
    // redis bitfield uses be so gotta swap em
    return Command({"BITFIELD"sv, fmt::format("block:{}", block_id), "SET"sv,
                    "i32"sv, "0"sv, std::to_string(bswap_32(confirmations))})
               .get() == nullptr;
}

#endif