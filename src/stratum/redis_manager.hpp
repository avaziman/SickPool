#ifndef REDIS_MANAGER_HPP_
#define REDIS_MANAGER_HPP_
#include <hiredis/hiredis.h>

#include <ctime>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "../config.hpp"
#define STRINGIFICATOR(X) #X

class RedisManager
{
   public:
    RedisManager(std::string coinSymbol, std::string host)
        : coin_symbol(coinSymbol)
    {
        rc = redisConnect("127.0.0.1", 6379);
        if (rc->err)
        {
            std::cerr << "Failed to connect to redis: " << rc->errstr
                      << std::endl;
        }
    }

    void AddWorker(std::string_view worker)
    {
        // redisReply *reply;

        redisAppendCommand(
            rc,
            "TS.CREATE " COIN_SYMBOL
            ":shares:%b"
            " RETENTION " STRINGIFICATOR(DB_RETENTION)  // time to live
            " ENCODING COMPRESSED"        // very data efficient
            " DUPLICATE_POLICY SUM"       // if two shares received at same ms
                                     // (rare), sum instead of ignoring (BLOCK)
            " LABELS coin " COIN_SYMBOL " type shares server IL worker %b ",
            worker.data(), worker.size(), worker.data(),
            worker.size());

        // std::string_view workerAddr = worker.substr(0, worker.find('.'));

        // redisAppendCommand(rc,
        //                    "TS.CREATE " COIN_SYMBOL
        //                    ":balance:%b"
        //                    " RETENTION 0"  // never expire balance log
        //                    " ENCODING COMPRESSED"
        //                    " LABELS coin " COIN_SYMBOL
        //                    " server IL address %b type balance",
        //                    workerAddr.data(), workerAddr.size(),
        //                    workerAddr.data(), workerAddr.size());

        // // reply for first TS.CREATE
        // if (!redisGetReply(rc, (void **)&reply) == REDIS_OK)
        // {
        //     std::cout << "Failed to add worker timeseries." << std::endl;
        // }
        // freeReplyObject(reply);

        // // reply for second TS.CREATE
        // if (!redisGetReply(rc, (void **)&reply) == REDIS_OK)
        // {
        //     std::cout << "Failed to add worker timeseries." << std::endl;
        // }
        // freeReplyObject(reply);
    }
    void AddShare(std::string_view worker, double diff, std::time_t time)
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
        }

        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)  // reply for TS.ADD
        {
            std::cerr << "Redis: Failed to append share (stats):" << std::endl;
        }
        freeReplyObject(reply);

        if (diff <= 0) return;

        // reply for HINCRBYFLOAT
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            std::cerr << "Redis: Failed to append share (round!):" << std::endl;
        }
        freeReplyObject(reply);
    }

    void CloseRound(uint32_t reward, uint32_t height)
    {
        // std::string finishedRound =
        //     coin_symbol + ":round:" + std::to_string(height);
        // rc.rename(coin_symbol + ":current_round", finishedRound);

        redisReply *hashReply, *balReply;

        redisAppendCommand(rc, "HGETALL " COIN_SYMBOL ":current_round");

        if (redisGetReply(rc, (void **)&hashReply) !=
            REDIS_OK)  // reply for TS.ADD
            std::cerr << "Redis: Failed to get current_round:" << std::endl;

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

            if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
            {
                std::cerr << "Redis: CRITICAL Failed to add balance! "
                          << std::endl;
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