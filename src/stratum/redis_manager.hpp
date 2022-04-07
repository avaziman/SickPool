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
    }

    bool AddWorker(std::string_view address, std::string_view worker_full)
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
    bool AddShare(std::string_view worker, std::time_t time, double diff)
    {
        redisReply *reply;
        redisAppendCommand(rc, "TS.ADD " COIN_SYMBOL ":shares:%b %lu %f",
                           worker.data(), worker.size(), time, diff);

        // only include valid shares in round total
        if (diff > 0)
        {
            // shares
            std::string_view address = worker.substr(0, worker.find('.'));
            redisAppendCommand(
                rc, "HINCRBYFLOAT " COIN_SYMBOL ":current_round %b %f",
                address.data(), address.size(), diff);

            redisAppendCommand(
                rc, "INCRBYFLOAT " COIN_SYMBOL ":current_round_effort %f", diff);
        }

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)  // reply for TS.ADD
        {
            std::cerr << "Redis: Failed to append share (stats):" << std::endl;
            return false;
        }
        freeReplyObject(reply);

        auto end = std::chrono::steady_clock::now();
        if (diff <= 0) return true;

        // reply for HINCRBYFLOAT
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            std::cerr << "Redis: Failed to append share (round!):" << std::endl;
            return false;
        }
        freeReplyObject(reply);

        // reply for INCRBYFLOAT
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            std::cerr << "Redis: Failed to append share (round_total!):"
                      << std::endl;
            return false;
        }
        freeReplyObject(reply);
        return true;
    }

    bool AddBlockSubmission(BlockSubmission submission, uint32_t height,
                            bool accepted, double totalDiff)
    {
        // KEY: TIME, VAL: WORKER:STATUS:HEIGHT:DIFF:HASH
        double effortPercent = totalDiff / submission.job->GetTargetDiff();
        double blockReward = submission.job->GetBlockReward();

        redisReply *reply;
        char hashHex[64];
        Hexlify(hashHex, (unsigned char *)submission.shareRes.HashBytes.data(),
                32);

        redisAppendCommand(
            rc, "ZADD " COIN_SYMBOL ":block_submissions %f %s:%u:%u:%f:%b",
            (double)submission.time, submission.worker.c_str(), (int)accepted,
            submission.shareRes.Diff, height, hashHex, sizeof(hashHex));

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to add block submission: %s", rc->errstr);
            return false;
        }

        freeReplyObject(reply);
        return true;
    }

    bool SetMatureTimestamp(std::time_t timestamp){
        redisReply *reply;
        redisAppendCommand(rc, "SET " COIN_SYMBOL ":mature_timestamp %lu",
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

    void CloseRound(uint32_t reward, uint32_t height)
    {
        // std::string finishedRound =
        //     coin_symbol + ":round:" + std::to_string(height);
        // rc.rename(coin_symbol + ":current_round", finishedRound);

        redisReply *hashReply, *balReply;

        redisAppendCommand(rc,
                           "RENAME " COIN_SYMBOL
                           ":current_round_effort " COIN_SYMBOL
                           ":round_total:%u",
                           height);

        if (redisGetReply(rc, (void **)&hashReply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to save effort!");
        }
        freeReplyObject(hashReply);

        redisAppendCommand(rc, "HGETALL " COIN_SYMBOL ":current_round");

        if (redisGetReply(rc, (void **)&hashReply) !=
            REDIS_OK)  // reply for TS.ADD
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to get current_round:%s", rc->errstr);

        double totalEffort = 0;
        for (int i = 0; i < hashReply->elements; i = i + 2)
        {
            hashReply->element[i + 1]->dval =
                std::strtod(hashReply->element[i + 1]->str, nullptr);
            totalEffort += hashReply->element[i + 1]->dval;
        }

        // PROP
        for (int i = 0; i < hashReply->elements; i = i + 2)
        {
            char *miner = hashReply->element[i]->str;
            double minerEffort = hashReply->element[i]->dval;

            double workShare = minerEffort / totalEffort;
            double minerReward = workShare * reward;

            redisAppendCommand(rc, "TS.ADD " COIN_SYMBOL ":balance:%s * %f",
                               miner, minerReward);
            // Todo: make this in another loop for 1 write and 1 recv
            if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::Redis,
                            "Failed to add balance! ");
            }
            freeReplyObject(balReply);
        }

        freeReplyObject(hashReply);
        // rc.hgetall("VRSC:current_round",
        //               std::inserter(result, result.end()));

        // double totalWork = 0;
        // for (int i = 0; i < result.size(); i++) totalWork +=
        // result[i].second;

        // for (int i = 0; i < result.size(); i++)
        // {
        //     // PROP
        //     std::string miner = result[i].first;
        //     double rewardShare = totalWork / result[i].second;
        //     rc.zadd(coin_symbol + ":payments",
        //                miner + ":" + std::to_string(rewardShare),
        //                height);

        //     std::cout << miner << " did " << rewardShare << "% of the
        //     work"
        //               << std::endl;
        // }
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

    double GetTotalEffort(){
        redisReply *reply;
        redisAppendCommand(rc, "GET " COIN_SYMBOL ":current_round_effort");

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to get current round effort: %s", rc->errstr);
            return 0;
        }

        double totalEffort = reply->dval;
        freeReplyObject(reply);
        return totalEffort;
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

   private:
    redisContext *rc;
    std::string coin_symbol;
};

#endif