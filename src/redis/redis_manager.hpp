#ifndef REDIS_MANAGER_HPP_
#define REDIS_MANAGER_HPP_
#include <byteswap.h>
#include <fmt/format.h>
#include <hiredis/hiredis.h>

#include <chrono>
#include <ctime>
#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "benchmark.hpp"
#include "blocks/block_submission.hpp"
#include "logger.hpp"
#include "payments/payment_manager.hpp"
#include "redis_transaction.hpp"
#include "round_share.hpp"
#include "shares/share.hpp"
#include "static_config/static_config.hpp"
#include "stats/stats.hpp"
#include "stats/stats_manager.hpp"
#include "utils.hpp"

#define xstr(s) str(s)
#define STRM(s) #s

typedef std::unique_ptr<redisReply, std::function<void(redisReply *)>>
    redis_unique_ptr;
struct TsAggregation
{
    std::string type;
    int64_t time_bucket_ms;
};

// couldn't make nested enums...pp
enum class Prefix
{
    POW,
    PAYOUTS,
    ADDRESS_MAP,
    PAYOUT_THRESHOLD,
    IDENTITY,
    JOIN_TIME,
    SCRIPT_PUB_KEY,
    ROUND_EFFORT,
    WORKER_COUNT,
    MINER_COUNT,
    TOTAL_EFFORT,
    ESTIMATED_EFFORT,
    ROUND_START_TIME,

    MATURE_BALANCE,
    IMMATURE_BALANCE,
    MATURE,
    IMMATURE,
    REWARD,

    HASHRATE,
    SHARES,

    BLOCK,

    NETWORK,
    POOL,

    AVERAGE,
    VALID,
    INVALID,
    STALE,

    EFFORT_PERCENT,

    SOLVER,
    INDEX,
    DURATION,
    EFFORT,
    DIFFICULTY,

    TYPE,
    NUMBER,
    CHAIN,
    STATS,
    COMPACT,
};

static constexpr std::string_view coin_symbol = COIN_SYMBOL;

// i couldn't make it with packed template...
template <Prefix T>
constexpr std::string_view PrefixKey()
{
    static constexpr std::string_view name = EnumName<T>();
    return join_v<coin_symbol, name>;
}

template <Prefix T1, Prefix T2>
constexpr std::string_view PrefixKey()
{
    static constexpr std::string_view name1 = EnumName<T1>();
    static constexpr std::string_view name2 = EnumName<T2>();
    return join_v<coin_symbol, name1, name2>;
}

template <Prefix T1, Prefix T2, Prefix T3>
constexpr std::string_view PrefixKey()
{
    static constexpr std::string_view name1 = EnumName<T1>();
    static constexpr std::string_view name2 = EnumName<T2>();
    static constexpr std::string_view name3 = EnumName<T3>();
    return join_v<coin_symbol, name1, name2, name3>;
}

template <Prefix T1, Prefix T2, Prefix T3, Prefix T4>
constexpr std::string_view PrefixKey()
{
    static constexpr std::string_view name1 = EnumName<T1>();
    static constexpr std::string_view name2 = EnumName<T2>();
    static constexpr std::string_view name3 = EnumName<T3>();
    static constexpr std::string_view name4 = EnumName<T4>();
    return join_v<coin_symbol, name1, name2, name3, name4>;
}

template <const std::string_view &...sview>
constexpr std::string_view PrefixKeySv()
{
    return join_v<coin_symbol, sview...>;
}

class RedisTransaction;
class RedisManager
{
    friend class RedisTransaction;

   public:
    RedisManager(const std::string &ip, const CoinConfig *cc);
    ~RedisManager();

    const RedisConfig *conf;

    /* block */

    void AppendAddBlockSubmission(const ExtendedSubmission *submission);
    bool UpdateBlockConfirmations(std::string_view block_id,
                                  int32_t confirmations);

    bool UpdateImmatureRewards(std::string_view chain, uint32_t block_num,
                               int64_t matured_time, bool matured);
    int GetBlockNumber();
    /* stats */

    bool AddNewMiner(std::string_view address, std::string_view addr_lowercase,
                     std::string_view worker_full, std::string_view idTag,
                     std::string_view script_pub_key, int64_t curtime);
    bool AddNewWorker(std::string_view address, std::string_view addr_lowercase,
                      std::string_view worker_full, std::string_view id_tag);
    // bool PopWorker(std::string_view address);

    /* round */
    bool LoadUnpaidRewards(
        std::vector<std::pair<std::string, PayeeInfo>> &rewards,
        const efforts_map_t &efforts, std::mutex *efforts_mutex);
    void AppendSetMinerEffort(std::string_view chain, std::string_view miner,
                              std::string_view type, double effort);
    void AppendAddRoundShares(std::string_view chain,
                              const BlockSubmission *submission,
                              const round_shares_t &miner_shares);
    bool SetNewBlockStats(std::string_view chain, int64_t curtime,
                          double net_hr, double estimated_shares);
    bool ResetMinersWorkerCounts(efforts_map_t &miner_stats_map,
                                 int64_t time_now);

    bool CloseRound(std::string_view chain, std::string_view type,
                    const ExtendedSubmission *submission,
                    round_shares_t &round_shares, int64_t time_ms);

    /* stats */
    bool LoadAverageHashrateSum(
        std::vector<std::pair<std::string, double>> &hashrate_sums,
        std::string_view prefix, int64_t hr_time);

    bool LoadMinersEfforts(std::string_view chain, std::string_view type,
                           efforts_map_t &efforts);

    bool UpdateEffortStats(efforts_map_t &miner_stats_map,
                           const double total_effort,
                           std::unique_lock<std::mutex> stats_mutex);

    bool UpdateIntervalStats(worker_map &worker_stats_map,
                             miner_map &miner_stats_map,
                             std::mutex *stats_mutex, double net_hr,
                             double diff, uint32_t blocks_found,
                             int64_t update_time_ms);
    bool TsMrange(std::vector<std::pair<std::string, double>> &last_averages,
                  std::string_view prefix, std::string_view type, int64_t from,
                  int64_t to, const TsAggregation *aggregation = nullptr);

    /* pos */
    bool AddStakingPoints(std::string_view chain, int64_t duration_ms);
    bool GetPosPoints(std::vector<std::pair<std::string, double>> &stakers,
                      std::string_view chain);

    /* payout */
    bool AddPayout(const PaymentInfo *payment);

    bool DoesAddressExist(std::string_view addrOrId, std::string &valid_addr);

    std::string hget(std::string_view key, std::string_view field);

    void LoadCurrentRound(std::string_view chain, std::string_view type,
                          Round *rnd);
    bool LoadImmatureBlocks(
        std::vector<std::unique_ptr<ExtendedSubmission>> &submsissions);

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

    std::mutex rc_mutex;

   private:
    redisContext *rc;
    int command_count = 0;
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

    void AppendTsCreate(std::string_view key, std::string_view prefix,
                        std::string_view type, std::string_view address,
                        std::string_view id, uint64_t retention_ms);

    bool hset(std::string_view key, std::string_view field,
              std::string_view val);
    void AppendHset(std::string_view key, std::string_view field,
                    std::string_view val);

    void AppendUpdateWorkerCount(std::string_view address, int amount,
                                 int64_t update_time_ms);
    void AppendCreateStatsTs(std::string_view addrOrWorker, std::string_view id,
                             std::string_view prefix,
                             std::string_view addr_lowercase_sv = "");
    void AppendTsAdd(std::string_view key_name, int64_t time, double value);

    void AppendIntervalStatsUpdate(std::string_view addr,
                                   std::string_view prefix,
                                   int64_t update_time_ms,
                                   const WorkerStats &ws);
};

#endif
