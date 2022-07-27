#ifndef REDIS_BLOCK_HPP
#define REDIS_BLOCK_HPP

#include "redis_manager.hpp"

void RedisManager::AppendAddBlockSubmission(const BlockSubmission *submission)
{
    std::scoped_lock lock(rc_mutex);

    uint32_t block_id = submission->number;
    auto chain = std::string((char *)submission->chain);

    {
        RedisTransaction add_block_tx(this);

        // serialize the block submission to save space and net
        // bandwidth, as the indexes are added manually anyway no need for
        // hash
        AppendCommand("SET block:%u %b", block_id, submission,
                      sizeof(BlockSubmission));
        /* sortable indexes */
        // block no. and block time will always be same order
        // so only one index is required to sort by either of them
        // (block num value is smaller)
        AppendCommand("ZADD block-index:number %f %u",
                      (double)submission->number, block_id);

        AppendCommand("ZADD block-index:reward %f %u",
                      (double)submission->block_reward, block_id);

        AppendCommand("ZADD block-index:difficulty %f %u",
                      submission->difficulty, block_id);

        AppendCommand("ZADD block-index:effort %f %u",
                      submission->effort_percent, block_id);

        AppendCommand("ZADD block-index:duration %f %u",
                      (double)submission->duration_ms, block_id);
        /* non-sortable indexes */
        AppendCommand("SADD block-index:chain:%s %u", chain.c_str(), block_id);

        AppendCommand("SADD block-index:type:PoW %u", block_id);

        AppendCommand("SADD block-index:solver:%b %u", submission->miner,
                      sizeof(submission->miner), block_id);

        AppendTsAdd(chain + ":round_effort_percent", submission->time_ms,
                    submission->effort_percent);
    }
}

bool RedisManager::UpdateBlockConfirmations(std::string_view block_id,
                                            int32_t confirmations)
{
    std::scoped_lock lock(rc_mutex);
    // redis bitfield uses be so gotta swap em
    return redisCommand(rc, "BITFIELD block:%b SET i32 0 %d", block_id.data(),
                        block_id.size(), bswap_32(confirmations)) == nullptr;
}

#endif