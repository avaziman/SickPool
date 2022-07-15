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

#include "round_manager.hpp"
#include "benchmark.hpp"
#include "blocks/block_submission.hpp"
#include "logger.hpp"
#include "payments/payment_manager.hpp"
#include "redis_transaction.hpp"
#include "shares/share.hpp"
#include "static_config/config.hpp"
#include "stats/stats.hpp"
#include "stats/stats_manager.hpp"

#define TOTAL_EFFORT_KEY "$total"
#define ESTIMATED_EFFORT_KEY "$estimated"

#define xstr(s) str(s)
#define STRM(s) #s

struct TsAggregation
{
    std::string type;
    int64_t time_bucket_ms;
};

class RedisTransaction;
class RedisManager
{
    friend class RedisTransaction;

   public:
    RedisManager(std::string ip, int port);

    /* block */
    bool AddBlockSubmission(const BlockSubmission *submission);
    bool UpdateBlockConfirmations(std::string_view block_id,
                                  int32_t confirmations);
    bool IncrBlockCount();
    uint32_t GetBlockNumber();
    bool UpdateImmatureRewards(std::string_view chain, uint32_t block_num,
                               int64_t matured_time, bool matured);

    /* stats */
    bool AddWorker(std::string_view address, std::string_view worker_full,
                   std::string_view idTag, int64_t curtime, bool newWorker,
                   bool newMiner);

    bool PopWorker(std::string_view address);

    /* round */
    bool SetRoundEstimatedEffort(std::string_view chain, std::string_view type,
                                 double effort);
    bool SetRoundStartTime(std::string_view chain, std::string_view type,
                           int64_t time);
    bool SetRoundEffort(std::string_view chain, std::string_view type,
                        double effort);
    int64_t GetRoundTime(std::string_view chain, std::string_view type);
    double GetRoundEffort(std::string_view chain, std::string_view type);
    bool SetMinerEffort(std::string_view chain, std::string_view miner,
                        std::string_view type, double effort);
    void AppendSetMinerEffort(std::string_view chain, std::string_view miner,
                              std::string_view type, double effort);
    bool ResetRoundEfforts(std::string_view chain, std::string_view type);
    bool AddRoundShares(std::string_view chain,
                        const BlockSubmission *submission,
                        const round_shares_t &miner_shares);
    // bool ResetMinersWorkerCounts(miner_map &miner_stats_map, int64_t time_now);

    bool CloseRound(std::string_view chain, std::string_view type,
                    const BlockSubmission *submission,
                    round_shares_t round_shares, int64_t time_ms);

    /* stats */
    bool LoadAverageHashrateSum(
        std::vector<std::pair<std::string, double>> &hashrate_sums,
        std::string_view prefix);

    bool LoadMinersEfforts(
        const std::string &chain,
        std::vector<std::pair<std::string, double>> &efforts);

    bool UpdateEffortStats(efforts_map_t &miner_stats_map,
                           const double total_effort, std::mutex *stats_mutex);

    bool UpdateIntervalStats(worker_map &worker_stats_map,
                             worker_map &miner_stats_map,
                             std::mutex *stats_mutex, int64_t update_time_ms);
    bool TsMrange(std::vector<std::pair<std::string, double>> &last_averages,
                  std::string_view prefix, std::string_view type, int64_t from,
                  int64_t to, TsAggregation *aggregation = nullptr);

    /* pos */
    bool AddStakingPoints(std::string_view chain, int64_t duration_ms);
    bool GetPosPoints(std::vector<std::pair<std::string, double>> &stakers,
                      std::string_view chain);

    void ClosePoSRound(int64_t roundStartMs, int64_t foundTimeMs,
                       int64_t reward, uint32_t height,
                       const double totalEffort, const double fee);
    // void UpdatePoS(uint64_t from, uint64_t maturity);

    bool DoesAddressExist(std::string_view addrOrId, std::string &valid_addr);

    int AddNetworkHr(std::string_view chain, int64_t time, double hr);
    bool TsCreate(
        std::string_view key_name, int retention,
        std::initializer_list<std::tuple<std::string_view, std::string_view>>
            labels);

   private:
    redisContext *rc;
    std::mutex rc_mutex;
    int command_count = 0;

    template <typename... Args>
    void AppendCommand(const char *str, Args... args)
    {
        // std::cout << command_count << "cnt: "  << str << std::endl;
        redisAppendCommand(rc, str, args...);
        command_count++;
    }

    inline bool GetReplies()
    {
        redisReply *reply;
        bool res = true;
        // std::cout << "clear " << command_count << std::endl;
        for (int i = 0; i < command_count; i++)
        {
            if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::Redis,
                            "Failed to get reply: {}", rc->errstr);
                res = false;
            }
            freeReplyObject(reply);
        }
        command_count = 0;
        return res;
    }

    bool hset(std::string_view key, std::string_view field,
              std::string_view val);
    void AppendHset(std::string_view key, std::string_view field,
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
