#ifndef REDIS_MANAGER_HPP_
#define REDIS_MANAGER_HPP_
#include <hiredis/hiredis.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "static_config/config.hpp"
#include "../logger.hpp"
#include "./share.hpp"
#include "block_submission.hpp"
#include "payment_manager.hpp"
#define xstr(s) str(s)
#define str(s) #s

// how we store stale and invalid shares in database

class RedisManager
{
   public:
    RedisManager()
    {
        rc = redisConnect("127.0.0.1", 6379);

        if (rc->err)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to connect to redis: %s", rc->errstr);
            exit(EXIT_FAILURE);
        }

        redisReply *reply;
        int command_count = 0;

        redisAppendCommand(
            rc,
            "TS.CREATE"
            " round_effort_percent"
            " RETENTION"
            " 0"                      // time to live
            " ENCODING COMPRESSED"   // very data efficient
            " DUPLICATE_POLICY MIN"  // min round 
        );
        command_count++;

        redisAppendCommand(
            rc,
            "TS.CREATE" 
            " worker_count"
            " RETENTION"
            " 0"                      // time to live
            " ENCODING COMPRESSED"   // very data efficient
            " DUPLICATE_POLICY SUM"  // sum to get total round effort
        );
        command_count++;

        redisAppendCommand(
            rc,
            "TS.CREATE "
            "miner_count"
            " RETENTION"
            " 0"                      // time to live
            " ENCODING COMPRESSED"   // very data efficient
            " DUPLICATE_POLICY ,om"  
        );
        command_count++;

        for (int i = 0; i < command_count; i++)
        {
            if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Error, LogField::Redis,
                            "Failed to create (%d) startup keys: %s", i,
                            rc->errstr);
                exit(EXIT_FAILURE);
            }
            freeReplyObject(reply);
        }
    }

    bool UpdateBlockConfirmations(std::string_view block_id, int32_t confirmations){
        // redis bitfield uses be so gotta swap em
        return redisCommand(rc, "BITFIELD block:%b SET i32 0 %d",
                            block_id.data(), block_id.size(),
                            bswap_32(confirmations)) == nullptr;
    }

    bool ResetWorkerCount()
    {
        redisReply *reply;
        auto chainName = std::string_view{"VRSCTEST"};

        redisAppendCommand(rc,
                           "ZREMRANGEBYRANK " COIN_SYMBOL ":worker_count 0 -1");

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to reset worker count: %s", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
        return true;
    }

    bool SetNewRoundTime(int64_t roundStart)
    {
        redisReply *reply;
        redisAppendCommand(rc,
                           "TS.ADD " COIN_SYMBOL ":round_effort %" PRId64 " 0",
                           roundStart);

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to set new round time: %s", rc->errstr);
            return false;
        }
        return true;
    }

    bool AddBlockSubmission(BlockSubmission &submission, bool accepted, const char* block_id)
    {
        int command_count = 0;

        if (!accepted)
        {
            submission.blockReward = 0;
        }

        redisAppendCommand(rc, "MULTI");
        command_count++;

        // serialize the block submission to save space and net
        // bandwidth, as the indexes are added manually anyway no need for hash
        redisAppendCommand(rc, "SET block:%s %b", block_id, &submission,
                           sizeof(submission));
        command_count++;

        /* sortable indexes */
        // block no. and block time will always be same order
        // so only one index is required to sort by either of them
        // (block num value is smaller)
        redisAppendCommand(rc, "ZADD block-index:number %f %s",
                           (double)submission.number, block_id);

        redisAppendCommand(rc, "ZADD block-index:reward %f %s",
                           (double)submission.blockReward, block_id);

        redisAppendCommand(rc, "ZADD block-index:difficulty %f %s",
                           submission.difficulty, block_id);

        redisAppendCommand(rc, "ZADD block-index:effort %f %s",
                           submission.effortPercent, block_id);

        redisAppendCommand(rc, "ZADD block-index:duration %f %s",
                           (double)submission.durationMs, block_id);
        command_count += 5;
        /* non-sortable indexes */
        redisAppendCommand(rc, "SADD block-index:chain:%b %s", submission.chain,
                           sizeof(submission.chain), block_id);

        redisAppendCommand(rc, "SADD block-index:type:PoW %s", block_id);

        redisAppendCommand(rc, "SADD block-index:solver:%b %s",
                           submission.miner, sizeof(submission.miner),
                           block_id);
        command_count += 3;

        redisAppendCommand(rc, "TS.ADD %b:round_effort_percent %" PRId64 " %f",
                           submission.chain, sizeof(submission.chain),
                           submission.timeMs, submission.effortPercent);
        command_count++;

        redisAppendCommand(rc, "EXEC");
        command_count++;

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

    bool SetMatureTimestamp(int64_t timestamp)
    {
        redisReply *reply;
        redisAppendCommand(rc, "SET " COIN_SYMBOL ":mature_timestamp %" PRId64,
                           timestamp);

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to set mature timestamp: %s", rc->errstr);
            return false;
        }

        freeReplyObject(reply);
        return true;
    }

    bool SetEstimatedNeededEffort(double effort)
    {
        redisReply *reply;
        redisAppendCommand(
            rc, "HSET " COIN_SYMBOL ":round_effort_pow estimated %f", effort);

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to set estimated needed effort: %s",
                        rc->errstr);
            return false;
        }

        freeReplyObject(reply);
        return true;
    }

    void ClosePoSRound(int64_t roundStartMs, int64_t foundTimeMs,
                       int64_t reward, uint32_t height,
                       const double totalEffort, const double fee)
    {
        redisReply *hashReply, *balReply;

        if (totalEffort == 0)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Total effort is 0, cannot close PoS round!");
            return;
        }

        redisAppendCommand(rc,
                           "TS.MRANGE %" PRId64 " %" PRId64
                           " FILTER type=balance coin=" COIN_SYMBOL,
                           roundStartMs, foundTimeMs);

        // reply for TS.MRANGE
        if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to get balances: %s", rc->errstr);
            return;
        }

        Logger::Log(LogType::Info, LogField::Redis,
                    "Closing round with %d balance changes",
                    hashReply->elements / 2);

        // separate append and get to pipeline (1 send, 1 recv!)
        for (int i = 0; i < hashReply->elements; i += 2)
        {
            char *miner = hashReply->element[i]->str;
            double minerEffort = std::stod(hashReply->element[i + 1]->str);

            double minerShare = minerEffort / totalEffort;
            int64_t minerReward = (int64_t)(reward * minerShare);
            minerReward -= minerReward * fee;  // substract fee

            redisAppendCommand(rc,
                               "TS.INCRBY " COIN_SYMBOL ":immature-balance:%s %" PRId64
                               " TIMESTAMP %" PRId64,
                               miner, minerReward, foundTimeMs);

            // round format: height:effort_percent:reward
            // NX = only new
            redisAppendCommand(
                rc,
                "ZADD " COIN_SYMBOL ":rounds:%s NX %" PRId64 " %u:%f:%" PRId64,
                miner, foundTimeMs, height, minerShare, minerReward);

            Logger::Log(LogType::Debug, LogField::Redis,
                        "Round: %d, miner: %s, effort: %f, share: %f, reward: "
                        "%" PRId64 ", total effort: %f",
                        height, miner, minerEffort, minerShare, minerReward,
                        totalEffort);
        }

        for (int i = 0; i < hashReply->elements; i += 2)
        {
            // reply for TS.INCRBY
            if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::Redis,
                            "Failed to increase balance: %s", rc->errstr);
            }
            freeReplyObject(balReply);

            if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::Redis,
                            "Failed to set round earning stats: %s",
                            rc->errstr);
            }

            freeReplyObject(balReply);
        }
        freeReplyObject(hashReply);  // free HGETALL reply
    }

    // TODO: review this
    bool DoesAddressExist(std::string_view addrOrId,
                          std::string_view &valid_addr)
    {
        redisReply *reply;
        redisAppendCommand(rc, "TS.QUERYINDEX address=%b", addrOrId.data(),
                           addrOrId.size());

        redisAppendCommand(rc, "TS.QUERYINDEX identity=%b", addrOrId.data(),
                           addrOrId.size());

        for (int i = 0; i < 2; i++)
        {
            if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Error, LogField::Redis,
                            "Failed to check if address/identity exists: %s",
                            rc->errstr);
                return false;
            }

            bool exists = reply->elements == 1;
            if (exists)
            {
                valid_addr = reply->element[0]->str +
                             sizeof(COIN_SYMBOL ":balance:") - 1;
                freeReplyObject(reply);
                return true;
            }
            freeReplyObject(reply);
        }

        return false;
    }

    int64_t GetLastRoundTimePow()
    {
        redisReply *reply;
        redisAppendCommand(rc, "TS.GET " COIN_SYMBOL ":round_effort");

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to get current round time stamp: %s",
                        rc->errstr);
            return 0;
        }
        else if (reply->elements != 2)
        {
            Logger::Log(
                LogType::Error, LogField::Redis,
                "Failed to get current round time stamp (wrong response)");
            freeReplyObject(reply);
            return 0;
        }

        int64_t roundStart = reply->element[0]->integer;
        freeReplyObject(reply);

        return roundStart;
    }

    uint32_t GetBlockCount()
    {
        // fix
        auto *reply =
            (redisReply *)redisCommand(rc, "GET " COIN_SYMBOL ":block_number");

        if (!reply)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to get block count: %s", rc->errstr);
            return 0;
        }
        else if (reply->type != REDIS_REPLY_INTEGER)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to get block count (wrong response)");
            freeReplyObject(reply);
            return 0;
        }

        uint32_t res = reply->integer;
        freeReplyObject(reply);

        return res;
    }

    bool IncrBlockCount()
    {
        redisReply *reply;
        redisAppendCommand(rc, "INCR block_number");

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to increment block count: %s", rc->errstr);
            return false;
        }

        freeReplyObject(reply);
        return true;
    }

    uint32_t GetJobCount()
    {
        // if (val)
        // {
        //     return static_cast<uint32_t>(std::stoul(*val));
        // }
        return 0;
    }

    void SetJobCount(uint32_t jobCount)
    {
        // bool res =
        //     rc.set(coin_symbol + ":job_count", std::to_string(jobCount));
        // if (!res) std::cerr << "Redis: Failed to update job count" <<
        // std::endl;
    }

    void AddNetworkHr(int64_t time, double hr)
    {
        redisReply *reply;
        redisAppendCommand(
            rc, "TS.ADD " COIN_SYMBOL ":network_difficulty %" PRId64 " %f",
            time, hr);
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to add network difficulty: %s", rc->errstr);
            return;
        }
        freeReplyObject(reply);
    }

    void UpdatePoS(uint64_t from, uint64_t maturity)
    {
        redisReply *reply;
        redisAppendCommand(rc,
                           "TS.MRANGE %" PRId64 " %" PRId64
                           " FILTER type=balance coin=" COIN_SYMBOL,
                           from, maturity);

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to get balances: %s", rc->errstr);
            return;
        }

        // assert type = arr (2)
        for (int i = 0; i < reply->elements; i++)
        {
            // key is 2d array of the following arrays: key name, (empty array -
            // labels), values (array of tuples)
            redisReply *key = reply->element[i];
            char *keyName = key[0].element[0]->str;

            // skip key name + null value
            redisReply *values = key[0].element[2];

            for (int j = 0; j < values->elements; j++)
            {
                int64_t timestamp = values->element[j]->element[0]->integer;
                char *value_str = values->element[j]->element[1]->str;
                int64_t value = std::stoll(value_str);
                std::cout << "timestamp: " << timestamp << " value: " << value
                          << " " << value_str << std::endl;
            }
        }
    }
    redisContext *rc;

   private:
    // redisContext *rc;
    std::string coin_symbol;
};

#endif
// TODO: onwalletnotify check if its pending block submission maybe check
// coinbase too and add it to block submission
