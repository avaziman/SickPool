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

#include "benchmark.hpp"
#include "blocks/block_submission.hpp"
#include "logger.hpp"
#include "payments/payment_manager.hpp"
#include "redis_transaction.hpp"
#include "round_manager.hpp"
#include "shares/share.hpp"
#include "static_config/config.hpp"
#include "stats/stats.hpp"
#include "stats/stats_manager.hpp"

#define xstr(s) str(s)
#define STRM(s) #s

typedef std::unique_ptr<redisReply, std::function<void(redisReply *)>>
    redis_unique_ptr;
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
    RedisManager(const std::string &ip, int port);
    ~RedisManager();

    /* block */

    void AppendAddBlockSubmission(const ExtendedSubmission *submission);
    bool UpdateBlockConfirmations(std::string_view block_id,
                                  int32_t confirmations);

    bool UpdateImmatureRewards(std::string_view chain, uint32_t block_num,
                               int64_t matured_time, bool matured);

    /* stats */
    bool AddNewMiner(std::string_view address, std::string_view worker_full,
                     std::string_view idTag, std::string_view script_pub_key,
                     int64_t curtime);
    bool AddNewWorker(std::string_view address, std::string_view worker_full,
                      std::string_view id_tag);
    bool PopWorker(std::string_view address);

    /* round */
    bool LoadUnpaidRewards(
        std::vector<std::pair<std::string, PayeeInfo>> &rewards,
        const efforts_map_t &efforts, std::mutex *efforts_mutex);
    void AppendSetMinerEffort(std::string_view chain, std::string_view miner,
                              std::string_view type, double effort);
    void AppendAddRoundShares(std::string_view chain,
                              const BlockSubmission *submission,
                              const round_shares_t &miner_shares);
    bool ResetMinersWorkerCounts(efforts_map_t &miner_stats_map,
                                 int64_t time_now);

    bool CloseRound(std::string_view chain, std::string_view type,
                    const ExtendedSubmission *submission,
                    round_shares_t round_shares, int64_t time_ms);

    /* stats */
    bool LoadAverageHashrateSum(
        std::vector<std::pair<std::string, double>> &hashrate_sums,
        std::string_view prefix);

    bool LoadMinersEfforts(std::string_view chain, std::string_view type,
                           efforts_map_t &efforts);

    bool UpdateEffortStats(efforts_map_t &miner_stats_map,
                           const double total_effort, std::mutex *stats_mutex);

    bool UpdateIntervalStats(worker_map &worker_stats_map,
                             worker_map &miner_stats_map,
                             std::mutex *stats_mutex, int64_t update_time_ms);
    bool TsMrange(std::vector<std::pair<std::string, double>> &last_averages,
                  std::string_view prefix, std::string_view type, int64_t from,
                  int64_t to, const TsAggregation *aggregation = nullptr);

    /* pos */
    bool AddStakingPoints(std::string_view chain, int64_t duration_ms);
    bool GetPosPoints(std::vector<std::pair<std::string, double>> &stakers,
                      std::string_view chain);

    bool DoesAddressExist(std::string_view addrOrId, std::string &valid_addr);

    int AddNetworkHr(std::string_view chain, int64_t time, double hr);
    std::string hget(std::string_view key, std::string_view field);

    void LoadCurrentRound(std::string_view chain, std::string_view type,
                          Round *rnd);

    inline bool GetReplies(redis_unique_ptr *last_reply = nullptr)
    {
        redisReply *reply;
        bool res = true;
        for (int i = 0; i < command_count - 1; i++)
        {
            if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::Redis,
                            "Failed to get reply: {}", rc->errstr);
                res = false;
            }

            freeReplyObject(reply);
        }

        if (command_count > 0 && redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to get reply: {}", rc->errstr);
            res = false;
        }

        if (last_reply)
        {
            *last_reply = redis_unique_ptr(reply, freeReplyObject);
        }
        else if (command_count > 0)
        {
            freeReplyObject(reply);
        }

        command_count = 0;
        return res;
    }

    static constexpr int ROUND_SHARES_LIMIT = 100;
    static constexpr std::string_view POW_KEY = "pow";
    static constexpr std::string_view BLOCK_NUMBER_KEY = "block-number";
    static constexpr std::string_view ADDRESS_MAP_KEY = "address-map";
    static constexpr std::string_view PAYOUT_THRESHOLD_KEY = "payout-threshold";
    static constexpr std::string_view IDENTITY_KEY = "identity";
    static constexpr std::string_view HASHRATE_KEY = "join-time";
    static constexpr std::string_view JOIN_TIME_KEY = "join-time";
    static constexpr std::string_view WORKER_COUNT_KEY = "worker-count";
    static constexpr std::string_view MATURE_BALANCE_KEY = "mature-balance";
    static constexpr std::string_view IMMATURE_BALANCE_KEY = "immature-balance";
    static constexpr std::string_view SCRIPT_PUB_KEY_KEY = "script-pub-key";
    static constexpr std::string_view ROUND_EFFORT_KEY = "round-effort";
    static constexpr std::string_view TOTAL_EFFORT_KEY = "$total";
    static constexpr std::string_view ESTIMATED_EFFORT_KEY = "$estimated";
    static constexpr std::string_view ROUND_START_TIME_KEY = "$estimated";

   private:
    redisContext *rc;
    int command_count = 0;
    std::mutex rc_mutex;

    void AppendCommand(std::initializer_list<std::string_view> args)
    {
        using namespace std::string_literals;
        const size_t argc = args.size();
        const char *argv[argc];
        size_t args_len[argc];
        int i = 0;

        for (const auto &arg : args)
        {
            args_len[i] = arg.size();
            argv[i] = arg.data();
            i++;
        }

        std::string prefixed_key;
        if (argc > 1)
        {
            prefixed_key =
                std::string(COIN_SYMBOL) + ":"s + std::string(argv[1]);
            // add coin prefix to the all keys
            argv[1] = prefixed_key.c_str();
            args_len[1] = prefixed_key.size();
        }

        redisAppendCommandArgv(rc, argc, argv, args_len);
        command_count++;
    }

    redis_unique_ptr Command(std::initializer_list<std::string_view> args)
    {
        AppendCommand(args);
        redis_unique_ptr rptr;
        bool res = GetReplies(&rptr);
        return rptr;
    }

    bool hset(std::string_view key, std::string_view field,
              std::string_view val);
    void AppendHset(std::string_view key, std::string_view field,
                    std::string_view val);

    void AppendUpdateWorkerCount(std::string_view address, int amount);
    void AppendCreateStatsTs(std::string_view addrOrWorker, std::string_view id,
                             std::string_view prefix);
    void AppendTsAdd(std::string_view key_name, int64_t time, double value);

    void AppendIntervalStatsUpdate(std::string_view addr,
                                   std::string_view prefix,
                                   int64_t update_time_ms,
                                   const WorkerStats &ws);
};

#endif
