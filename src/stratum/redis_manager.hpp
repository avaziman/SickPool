#ifndef REDIS_MANAGER_HPP_
#define REDIS_MANAGER_HPP_
#include <hiredis/hiredis.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "../config.hpp"
#include "../logger.hpp"
#include "./share.hpp"
#include "payment_manager.hpp"
#define xstr(s) str(s)
#define str(s) #s

// how we store stale and invalid shares in database
#define STALE_SHARE_DIFF -1
#define INVALID_SHARE_DIFF -2

class RedisManager
{
   public:
    RedisManager(std::string_view host)
    {
        rc = redisConnect("127.0.0.1", 6379);

        if (rc->err)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to connect to redis: %s", rc->errstr);
            exit(EXIT_FAILURE);
        }
        // increase performance by not freeing buffer
        // rc->reader->maxbuf = 0;

        redisReply *reply;

        redisAppendCommand(
            rc,
            "TS.CREATE " COIN_SYMBOL
            ":round_effort"
            " RETENTION "
            "0"                      // time to live
            " ENCODING COMPRESSED"   // very data efficient
            " DUPLICATE_POLICY SUM"  // sum to get total round effort
        );

        redisAppendCommand(
            rc,
            "TS.CREATE " COIN_SYMBOL
            ":round_effort_percent"
            " RETENTION "
            "0"                      // time to live
            " ENCODING COMPRESSED"   // very data efficient
            " DUPLICATE_POLICY SUM"  // sum to get total round effort
        );

        redisAppendCommand(
            rc,
            "TS.CREATE " COIN_SYMBOL
            ":worker_count"
            " RETENTION "
            "0"                      // time to live
            " ENCODING COMPRESSED"   // very data efficient
            " DUPLICATE_POLICY SUM"  // sum to get total round effort
        );

        redisAppendCommand(
            rc,
            "TS.CREATE " COIN_SYMBOL
            ":miner_count"
            " RETENTION "
            "0"                      // time to live
            " ENCODING COMPRESSED"   // very data efficient
            " DUPLICATE_POLICY SUM"  // sum to get total round effort
        );

        redisAppendCommand(
            rc,
            "FT.CREATE " COIN_SYMBOL
            ":block_index ON HASH PREFIX 1 " COIN_SYMBOL
            ":block:* SCHEMA worker TAG SORTABLE height "
            "NUMERIC SORTABLE difficulty NUMERIC SORTABLE reward" COIN_SYMBOL
            " NUMERIC SORTABLE");

        for (int i = 0; i < 5; i++)
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

    bool AddWorker(std::string_view address, std::string_view worker_full,
                   std::time_t curtime)
    {
        redisReply *reply;

        redisAppendCommand(
            rc,
            "TS.CREATE " COIN_SYMBOL
            ":shares:%b"
            " RETENTION " xstr(DB_RETENTION)  // time to live
            " ENCODING COMPRESSED"            // very data efficient
            " DUPLICATE_POLICY SUM"  // if two shares received at same ms
                                     // (rare), sum instead of ignoring (BLOCK)
            " LABELS coin " COIN_SYMBOL " type shares server IL worker %b",
            worker_full.data(), worker_full.size(), worker_full.data(),
            worker_full.size());

        redisAppendCommand(rc,
                           "TS.CREATE " COIN_SYMBOL
                           ":hashrate:%b"
                           " RETENTION " xstr(DB_RETENTION)  // time to live
                           " ENCODING COMPRESSED"  // very data efficient
                           " LABELS coin " COIN_SYMBOL
                           " type hashrate server IL address %b worker %b",
                           worker_full.data(), worker_full.size(),
                           address.data(), address.size(), worker_full.data(),
                           worker_full.size());

        redisAppendCommand(rc,
                           "TS.CREATERULE " COIN_SYMBOL
                           ":shares:%b "  // source key
                           COIN_SYMBOL
                           ":hashrate:%b"  // dest key
                           " AGGREGATION SUM " xstr(HASHRATE_PERIOD),
                           worker_full.data(), worker_full.size(),
                           worker_full.data(), worker_full.size());

        for (int i = 0; i < 3; i++)
        {
            if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Error, LogField::Redis,
                            "Failed to add worker (%d) timeseries: %s", i,
                            rc->errstr);
                return false;
            }
            freeReplyObject(reply);
        }

        return true;
    }

    bool AddAddress(std::string_view addr, std::string_view identity)
    {
        redisReply *reply;
        redisAppendCommand(rc,
                           "TS.CREATE " COIN_SYMBOL
                           ":balance:%b"
                           " RETENTION 0"  // never expire balance log
                           " ENCODING COMPRESSED"
                           " LABELS coin " COIN_SYMBOL
                           " type balance server IL address %b identity %b",
                           addr.data(), addr.size(), addr.data(), addr.size(),
                           identity.data(), identity.size());

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to add address (balance) timeseries: %s",
                        rc->errstr);
            return false;
        }
        freeReplyObject(reply);

        return true;
    }
    bool AddShare(std::string_view worker, int64_t shareTime,
                  int64_t roundStart, double diff)
    {
        redisReply *reply;
        redisAppendCommand(rc,
                           "TS.ADD " COIN_SYMBOL ":shares:%b %" PRId64 " %f",
                           worker.data(), worker.size(), shareTime, diff);

        // only include valid shares in round total
        if (diff > 0)
        {
            // shares
            std::string_view address = worker.substr(0, worker.find('.'));
            redisAppendCommand(
                rc, "HINCRBYFLOAT " COIN_SYMBOL ":current_round %b %f",
                address.data(), address.size(), diff);

            // this will incrby as we have sum duplicate policy
            redisAppendCommand(
                rc, "TS.ADD " COIN_SYMBOL ":round_effort %" PRId64 " %f",
                roundStart, diff);
        }

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)  // reply for TS.ADD
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Redis: Failed to append share (stats): %s",
                        rc->errstr);
            return false;
        }
        freeReplyObject(reply);

        auto end = std::chrono::steady_clock::now();
        if (diff > 0)
        {
            // reply for HINCRBYFLOAT
            if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::Redis,
                            "Redis: Failed to append share (round!): %s",
                            rc->errstr);
                return false;
            }
            freeReplyObject(reply);

            // reply for TS.ADD
            if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::Redis,
                            "Redis: Failed to append share (round_total!): %s",
                            rc->errstr);
                return false;
            }
            freeReplyObject(reply);
        }
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

    bool AddBlockSubmission(BlockSubmission submission, bool accepted,
                            double totalDiff)
    {
        const double effortPercent =
            (totalDiff / submission.job->GetTargetDiff()) * 100;

        redisReply *reply;

        redisAppendCommand(
            rc,
            "HSET " COIN_SYMBOL ":block:%d height %d accepted %s time %" PRId64
            " worker %b "
            "reward" COIN_SYMBOL " %" PRId64
            " difficulty %f total_effort %f effort_percent %f hash %b",
            submission.height, submission.height, BoolToCstring(accepted),
            submission.timeMs, submission.worker.data(),
            submission.worker.size(), submission.job->GetBlockReward(),
            submission.shareRes.Diff, totalDiff, effortPercent,
            submission.hashHex, HASH_SIZE * 2);

        redisAppendCommand(
            rc, "TS.ADD " COIN_SYMBOL ":round_effort_percent %" PRId64 " %f",
            submission.timeMs, effortPercent);

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to add block submission: %s", rc->errstr);
            return false;
        }
        freeReplyObject(reply);

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to add round effort percent: %s", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
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

    void ClosePoWRound(int64_t timeMs, int64_t reward, uint32_t height,
                       const double totalEffort, const double fee)
    {
        redisReply *hashReply, *balReply;

        if (totalEffort == 0)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Total effort is 0, cannot close round!");
            return;
        }

        redisAppendCommand(rc, "HGETALL " COIN_SYMBOL ":round:%d", height);

        // reply for HGETALL
        if (redisGetReply(rc, (void **)&hashReply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to get current_round: %s", rc->errstr);
            return;
        }

        Logger::Log(LogType::Info, LogField::Redis,
                    "Closing round with %d workers", hashReply->elements / 2);

        // separate append and get to pipeline (1 send, 1 recv!)
        for (int i = 0; i < hashReply->elements; i += 2)
        {
            char *miner = hashReply->element[i]->str;
            double minerEffort = std::stod(hashReply->element[i + 1]->str);

            double minerShare = minerEffort / totalEffort;
            int64_t minerReward = (int64_t)(reward * minerShare);
            minerReward -= minerReward * fee;  // substract fee

            redisAppendCommand(rc,
                               "TS.INCRBY " COIN_SYMBOL ":balance:%s %" PRId64
                               " TIMESTAMP %" PRId64,
                               miner, minerReward, timeMs);

            // round format: height:effort_percent:reward
            // NX = only new
            redisAppendCommand(rc,
                               "ZADD " COIN_SYMBOL ":rounds:%s NX %" PRId64
                               " %u:%f:%" PRId64,
                               miner, timeMs, height, minerShare, minerReward);

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

    bool RenameCurrentRound(uint32_t height)
    {
        redisReply *reply;
        redisAppendCommand(
            rc, "RENAME " COIN_SYMBOL ":current_round " COIN_SYMBOL ":round:%d",
            height);
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            // if this is not fixed, more money than needed will be paid
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to get remove current round!", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
        return true;
    }

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

    // double GetTotalEffort(int64_t last_round_start)
    // {
    //     redisReply *reply;
    //     redisAppendCommand(rc, "TS.RANGE " COIN_SYMBOL ":round_effort %"
    //     PRId64 " %" PRId64,
    //                        last_round_start, last_round_start);

    //     if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
    //     {
    //         Logger::Log(LogType::Error, LogField::Redis,
    //                     "Failed to get current round effort: %s",
    //                     rc->errstr);
    //         return 0;
    //     }
    //     else if (reply->type != REDIS_REPLY_ARRAY || reply->elements != 2)
    //     {
    //         Logger::Log(
    //             LogType::Error, LogField::Redis,
    //             "Failed to get current round effort (wrong response): %s",
    //             reply->str);
    //         freeReplyObject(reply);
    //         return 0;
    //     }
    //     double totalEffort = std::stod(reply->element[1]->str);
    //     freeReplyObject(reply);

    //     return totalEffort;
    // }

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

   private:
    redisContext *rc;
    std::string coin_symbol;
};

#endif