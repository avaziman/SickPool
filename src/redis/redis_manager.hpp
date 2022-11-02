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
#include "coin_config.hpp"
#include "logger.hpp"
#include "payments/round_share.hpp"
#include "redis_transaction.hpp"
#include "shares/share.hpp"
#include "static_config/static_config.hpp"
#include "stats/stats.hpp"
#include "utils.hpp"

#define STRR(s) #s
#define xSTRR(s) STRR(s)

typedef std::unique_ptr<redisReply, std::function<void(redisReply *)>>
    redis_unique_ptr;
typedef std::unique_ptr<redisContext, std::function<void(redisContext *)>>
    redis_unique_ptr_context;

typedef std::vector<std::pair<std::string, PayeeInfo>> payees_info_t;

struct TsAggregation
{
    std::string type;
    int64_t time_bucket_ms;
};

// couldn't make nested enums...pp
enum class Prefix
{
    POW,
    PAYOUT,
    PAYOUT_FEELESS,
    PAYOUTS,
    ADDRESS_MAP,
    ALIAS_MAP,
    PAYOUT_THRESHOLD,
    IDENTITY,
    ROUND,
    EFFORT,
    WORKER_COUNT,
    MINER_COUNT,
    TOTAL_EFFORT,
    ESTIMATED_EFFORT,
    START_TIME,

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
    DIFFICULTY,
    ROUND_EFFORT,

    PAYEES,
    FEE_PAYEES,
    PENDING_AMOUNT,
    PENDING_AMOUNT_FEE,
    PENDING,
    FEELESS,
    MINER,
    WORKER,
    TYPE,
    NUMBER,
    CHAIN,
    STATS,
    COMPACT,
};

// TODO: update pending payout on payment settings change
static constexpr std::string_view coin_symbol = COIN_SYMBOL;

class RedisTransaction;
class RedisManager
{
    friend class RedisTransaction;

   public:
    RedisManager(const std::string &ip, const CoinConfig *cc, int db_index = 0);
    RedisManager(const RedisManager &rm)
        : conf(rm.conf), logger(rm.logger), key_names(rm.key_names)
    {
    }
    ~RedisManager();
    void Init();
    /* block */

    // bool GetPendingPayouts(reward_map_t &payouts) {
    //     AppendCommand()
    // }

    // manual binary search :)

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
    bool LoadUnpaidRewards(payees_info_t &rewards,
                           const std::vector<std::string> &addresses);
    bool GetAddresses(std::vector<std::string> &addresses);

    /* stats */
    bool SetNewBlockStats(std::string_view chain, int64_t curtime,
                          double net_hr, double estimated_shares);
    bool ResetMinersWorkerCounts(efforts_map_t &miner_stats_map,
                                 int64_t time_now);
    bool LoadAverageHashrateSum(
        std::vector<std::pair<std::string, double>> &hashrate_sums,
        std::string_view prefix, int64_t hr_time);

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

    bool LoadImmatureBlocks(
        std::vector<std::unique_ptr<ExtendedSubmission>> &submsissions);

    bool UpdateBlockNumber(int64_t time, uint32_t number);

    inline bool GetReplies(redis_unique_ptr *last_reply = nullptr)
    {
        redisReply *reply;
        bool res = true;
        for (int i = 0; i < command_count - 1; i++)
        {
            if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
            {
                logger.Log<LogType::Critical>("Failed to get reply: {}",
                                              rc->errstr);
                res = false;
            }

            freeReplyObject(reply);
        }

        if (command_count > 0 && redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            logger.Log<LogType::Critical>("Failed to get reply: {}",
                                          rc->errstr);
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

    bool AddPendingPayees(payees_info_t &payees)
    {
        using enum Prefix;
        AppendCommand({"UNLINK", key_names.pending_payout});

        int total_payees = 0, fee_payees = 0;
        int64_t pending_amount = 0, pending_amount_fee = 0;

        for (const auto &[addr, payee_info] : payees)
        {
            total_payees++;
            pending_amount += payee_info.amount;
            if (payee_info.settings.pool_block_only)
            {
                // minus represents feeless
                AppendCommand({"HSET", key_names.pending_payout, addr,
                               std::to_string(-payee_info.amount)});
            }
            else
            {
                AppendCommand({"HSET", key_names.pending_payout, addr,
                               std::to_string(payee_info.amount)});
                pending_amount_fee += payee_info.amount;
                fee_payees++;
            }
        }

        AppendCommand({"HSET", key_names.pending_payout, EnumName<PAYEES>(),
                       std::to_string(total_payees)});
        AppendCommand({"HSET", key_names.pending_payout, EnumName<FEE_PAYEES>(),
                       std::to_string(fee_payees)});

        AppendCommand({"HSET", key_names.pending_payout,
                       EnumName<PENDING_AMOUNT>(),
                       std::to_string(pending_amount)});
        AppendCommand({"HSET", key_names.pending_payout,
                       EnumName<PENDING_AMOUNT_FEE>(),
                       std::to_string(pending_amount_fee)});

        return GetReplies();
    }

    static constexpr int ROUND_SHARES_LIMIT = 100;

    static redis_unique_ptr_context rc_unique;
    static redisContext *rc;
    static std::mutex rc_mutex;
    const CoinConfig *conf;

    // TODO: make private...
    struct KeyNames
    {
       private:
        const std::string coin;

       public:
        explicit KeyNames(std::string_view coin) : coin(coin) {}
        using enum Prefix;

        const std::string round = Format({coin, EnumName<ROUND>()});
        const std::string round_shares = Format({round, EnumName<SHARES>()});
        const std::string round_efforts = Format({round, EnumName<EFFORT>()});

        const std::string block = Format({coin, EnumName<BLOCK>()});
        const std::string block_effort_percent =
            Format({block, EnumName<EFFORT_PERCENT>()});
        const std::string block_effort_percent_compact =
            Format({block_effort_percent, EnumName<COMPACT>()});
        const std::string block_number_compact =
            Format({block, EnumName<NUMBER>(), EnumName<COMPACT>()});

        // derived
        const std::string block_index = Format({block, EnumName<INDEX>()});
        const std::string block_index_number =
            Format({block_index, EnumName<NUMBER>()});
        const std::string block_index_reward =
            Format({block_index, EnumName<REWARD>()});
        const std::string block_index_difficulty =
            Format({block_index, EnumName<DIFFICULTY>()});
        const std::string block_index_effort =
            Format({block_index, EnumName<EFFORT>()});
        const std::string block_index_duration =
            Format({block_index, EnumName<DURATION>()});
        const std::string block_index_chain =
            Format({block_index, EnumName<CHAIN>()});
        const std::string block_index_solver =
            Format({block_index, EnumName<SOLVER>()});
        const std::string block_mature_channel =
            Format({block, EnumName<MATURE>()});

        const std::string shares = Format({coin, EnumName<SHARES>()});
        const std::string shares_valid = Format({shares, EnumName<VALID>()});
        const std::string shares_stale = Format({shares, EnumName<STALE>()});
        const std::string shares_invalid =
            Format({shares, EnumName<INVALID>()});

        const std::string hashrate = Format({coin, EnumName<HASHRATE>()});
        const std::string hashrate_average =
            Format({hashrate, EnumName<AVERAGE>()});
        const std::string hashrate_network =
            Format({hashrate, EnumName<NETWORK>()});
        const std::string hashrate_network_compact =
            Format({hashrate_network, EnumName<COMPACT>()});
        const std::string hashrate_pool = Format({hashrate, EnumName<POOL>()});
        const std::string hashrate_pool_compact =
            Format({hashrate_pool, EnumName<COMPACT>()});

        const std::string worker_count =
            Format({coin, EnumName<WORKER_COUNT>()});
        const std::string worker_count_pool =
            Format({worker_count, EnumName<POOL>()});
        const std::string worker_countp_compact =
            Format({worker_count_pool, EnumName<COMPACT>()});

        const std::string miner_count =
            Format({coin, EnumName<MINER_COUNT>(), EnumName<POOL>()});
        const std::string miner_count_compact =
            Format({miner_count, EnumName<COMPACT>()});

        const std::string difficulty = Format({coin, EnumName<DIFFICULTY>()});
        const std::string difficulty_compact =
            Format({difficulty, EnumName<COMPACT>()});

        const std::string solver = Format({coin, EnumName<SOLVER>()});
        const std::string solver_index = Format({solver, EnumName<INDEX>()});
        const std::string solver_index_mature =
            Format({solver_index, EnumName<MATURE_BALANCE>()});
        const std::string solver_index_worker_count =
            Format({solver_index, EnumName<WORKER_COUNT>()});
        const std::string solver_index_hashrate =
            Format({solver_index, EnumName<HASHRATE>()});
        const std::string solver_index_jointime =
            Format({solver_index, EnumName<START_TIME>()});

        const std::string reward = Format({coin, EnumName<REWARD>()});
        const std::string reward_immature =
            Format({reward, EnumName<IMMATURE>()});
        const std::string reward_mature = Format({reward, EnumName<MATURE>()});

        const std::string address_map = Format({coin, EnumName<ADDRESS_MAP>()});
        const std::string block_number =
            Format({coin, EnumName<BLOCK>(), EnumName<NUMBER>()});

        const std::string payout = Format({coin, EnumName<PAYOUT>()});
        const std::string pending_payout =
            Format({payout, EnumName<PENDING>()});
    };
    const KeyNames key_names;

   protected:
    static constexpr std::string_view logger_field = "Redis";
    const Logger<logger_field> logger;
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

        redisAppendCommandArgv(rc, static_cast<int>(argc), argv, args_len);
        command_count++;
    }

    redis_unique_ptr Command(std::initializer_list<std::string_view> args)
    {
        AppendCommand(args);
        redis_unique_ptr rptr;
        bool res = GetReplies(&rptr);
        return rptr;
    }

    struct Stringable
    {
        const std::string_view val;

        // explicit(false) Stringable(Prefix p) : val(STRR(p)) {}
        explicit(false) Stringable(std::string_view p) : val(p) {}
        explicit(false) Stringable(const std::string &p) : val(p) {}

        explicit(false) operator std::string_view() const { return val; }
    };

    using Args = std::initializer_list<Stringable>;

    static std::string Format(Args args)
    {
        // assert(args.size() > 0);

        std::string res;
        for (const auto &a : args)
        {
            res += a;
            res += ':';
        }

        res.pop_back();

        return res;
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

   private:
    int command_count = 0;
};

#endif
