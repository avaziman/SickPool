#ifndef REDIS_BLOCK_HPP
#define REDIS_BLOCK_HPP

#include "redis_manager.hpp"

bool RedisManager::AddBlockSubmission(const BlockSubmission *submission)
{
    std::scoped_lock lock(rc_mutex);

    int command_count = 0;
    uint32_t block_id = submission->number;
    auto chain =
        std::string((char *)submission->chain, sizeof(submission->chain));

    {
        RedisTransaction add_block_tx(rc, command_count);

        // serialize the block submission to save space and net
        // bandwidth, as the indexes are added manually anyway no need for
        // hash
        redisAppendCommand(rc, "SET block:%u %b", block_id, submission,
                           sizeof(BlockSubmission));
        command_count++;

        /* sortable indexes */
        // block no. and block time will always be same order
        // so only one index is required to sort by either of them
        // (block num value is smaller)
        redisAppendCommand(rc, "ZADD block-index:number %f %u",
                           (double)submission->number, block_id);
        command_count++;

        redisAppendCommand(rc, "ZADD block-index:reward %f %u",
                           (double)submission->blockReward, block_id);
        command_count++;

        redisAppendCommand(rc, "ZADD block-index:difficulty %f %u",
                           submission->difficulty, block_id);
        command_count++;

        redisAppendCommand(rc, "ZADD block-index:effort %f %u",
                           submission->effortPercent, block_id);
        command_count++;

        redisAppendCommand(rc, "ZADD block-index:duration %f %u",
                           (double)submission->durationMs, block_id);
        command_count++;
        /* non-sortable indexes */
        redisAppendCommand(rc, "SADD block-index:chain:%s %u", chain.c_str(),
                           block_id);
        command_count++;

        redisAppendCommand(rc, "SADD block-index:type:PoW %u", block_id);
        command_count++;

        redisAppendCommand(rc, "SADD block-index:solver:%b %u",
                           submission->miner, sizeof(submission->miner),
                           block_id);
        command_count++;

        command_count +=
            AppendTsAdd(chain + ":round_effort_percent", submission->timeMs,
                        submission->effortPercent);
    }

    redisReply *reply;
    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to add block submission and indexes: %s",
                        rc->errstr);
            return false;
        }
        freeReplyObject(reply);
    }
    return true;
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