#ifndef REDIS_MANAGER_HPP_
#define REDIS_MANAGER_HPP_
#include <fmt/format.h>
#include <hiredis/hiredis.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "blocks/block_submission.hpp"
#include "logger.hpp"
#include "payments/payment_manager.hpp"
#include "redis_transaction.hpp"
#include "shares/share.hpp"
#include "static_config/config.hpp"
#include "stats/stats.hpp"
#include "stats/stats_manager.hpp"

#define xstr(s) str(s)
#define str(s) #s

// how we store stale and invalid shares in database
class RedisManager
{
   public:
    RedisManager(std::string ip, int port);

    /* block */
    bool AddBlockSubmission(const BlockSubmission *submission);
    bool UpdateBlockConfirmations(std::string_view block_id,
                                  int32_t confirmations);
    bool IncrBlockCount();
    uint32_t GetBlockNumber();
    bool UpdateImmatureRewards(std::string_view chain, int64_t rewardTime,
                               bool matured);

    /* stats */
    bool AddWorker(std::string_view address, std::string_view worker_full,
                   std::string_view idTag, int64_t curtime, bool newWorker,
                   bool newMiner);

    bool PopWorker(std::string_view address);

    /* pow round */
    bool SetRoundEstimatedEffort(std::string_view chain, double effort);
    bool SetRoundTimePow(std::string_view chain, int64_t time);
    bool SetRoundEffortPow(std::string_view chain, double effort);
    int64_t GetRoundTimePow(std::string_view chain);
    double GetRoundEffortPow(std::string_view chain);
    bool AddMinerShares(std::string_view chain,
                        const BlockSubmission *submission,
                        std::vector<RoundShare> &miner_shares);

    /* stats */
    bool LoadSolverStats(miner_map &miner_stats_map, round_map &round_map);
    bool UpdateStats(worker_map worker_stats, miner_map miner_stats,
                     int64_t update_time_ms, bool update_interval,
                     bool update_effort);

    void ClosePoSRound(int64_t roundStartMs, int64_t foundTimeMs,
                       int64_t reward, uint32_t height,
                       const double totalEffort, const double fee);

    bool DoesAddressExist(std::string_view addrOrId, std::string &valid_addr);

    int AddNetworkHr(std::string_view chain, int64_t time, double hr);

    void UpdatePoS(uint64_t from, uint64_t maturity);

   private:
    redisContext *rc;
    std::mutex rc_mutex;
    std::string coin_symbol;
    int command_count = 0;

    template <typename... Args>
    inline void AppendCommand(const char *str, Args... args)
    {
        redisAppendCommand(rc, str, args...);
        command_count++;
    }

    inline bool GetReplies()
    {
        redisReply *reply;
        bool res = true;
        for (int i = 0; i < command_count; i++)
        {
            if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::Redis,
                            "Failed to get reply: {}\n", rc->errstr);
                res = false;
            }
            freeReplyObject(reply);
        }
        command_count = 0;
        return res;
    }

    bool hset(std::string_view key, std::string_view field,
              std::string_view val);

    int64_t hgeti(std::string_view key, std::string_view field);

    double hgetd(std::string_view key, std::string_view field);

    void AppendUpdateWorkerCount(std::string_view address, int amount);
    void AppendCreateStatsTs(std::string_view addrOrWorker, std::string_view id,
                             std::string_view prefix);
    void AppendTsCreate(
        std::string_view key_name, int retention,
        std::initializer_list<std::tuple<std::string_view, std::string_view>>
            labels);

    void AppendTsAdd(std::string_view key_name, int64_t time, double value);

    void AppendIntervalStatsUpdate(std::string_view addr,
                                   std::string_view prefix,
                                   int64_t update_time_ms,
                                   const WorkerStats &ws);
};

#endif
// TODO: onwalletnotify check if its pending block submission maybe check
// coinbase too and add it to block submission
