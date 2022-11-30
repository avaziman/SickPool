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

#include "redis_interop.hpp"
#include "benchmark.hpp"
#include "coin_config.hpp"
#include "key_names.hpp"
#include "logger.hpp"
#include "payments/round_share.hpp"
#include "redis_transaction.hpp"
#include "shares/share.hpp"
#include "static_config/static_config.hpp"
#include "stats/stats.hpp"
#include "utils.hpp"

#define STRR(s) #s
#define xSTRR(s) STRR(s)

using redis_unique_ptr =
    std::unique_ptr<redisReply, std::function<void(redisReply *)>>;
using redis_unique_ptr_context =
    std::unique_ptr<redisContext, std::function<void(redisContext *)>>;

using payees_info_t = std::vector<std::pair<std::string, PayeeInfo>>;

struct TsAggregation
{
    std::string type;
    int64_t time_bucket_ms;
};

// TODO: update pending payout on payment settings change

class RedisTransaction;
class RedisManager
{
    friend class RedisTransaction;

   public:
    explicit RedisManager(const std::string &ip, const CoinConfig *cc,
                          int db_index = 0);
    explicit RedisManager(const RedisManager &rm)
        : conf(rm.conf), key_names(rm.key_names)
    {
    }
    ~RedisManager();
    void Init();
    /* block */

    // bool GetPendingPayouts(reward_map_t &payouts) {
    //     AppendCommand()
    // }

    // manual binary search :)
    bool GetActiveIds(std::vector<MinerIdHex> &addresses);
    bool SetActiveId(const MinerIdHex &id);
    void AppendAddBlockSubmission(const BlockSubmission *submission);
    bool UpdateBlockConfirmations(std::string_view block_id,
                                  int32_t confirmations);

    bool UpdateImmatureRewards(uint8_t chain, uint32_t block_num,
                               int64_t matured_time, bool matured);
    uint32_t GetBlockNumber();
    /* stats */

    /* round */
    bool LoadUnpaidRewards(payees_info_t &rewards,
                           const std::vector<MinerIdHex> &active_ids);

    /* stats */

    bool TsMrange(std::vector<std::pair<WorkerFullId, double>> &last_averages,
                  std::string_view prefix, std::string_view type, int64_t from,
                  int64_t to, const TsAggregation *aggregation = nullptr);

    /* pos */
    bool AddStakingPoints(std::string_view chain, int64_t duration_ms);
    bool GetPosPoints(std::vector<std::pair<std::string, double>> &stakers,
                      std::string_view chain);

    /* payout */
    // bool AddPayout(const PaymentInfo *payment);

    std::string hget(std::string_view key, std::string_view field);

    bool LoadImmatureBlocks(
        std::vector<std::unique_ptr<BlockSubmission>> &submsissions);

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

            //TODO: handle err

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

    const KeyNames key_names;

   protected:
    static constexpr std::string_view logger_field = "Redis";
    static const Logger<logger_field> logger;

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

    void AppendTsCreate(std::string_view key, std::string_view prefix,
                        std::string_view type, std::string_view address,
                        std::string_view id, uint64_t retention_ms,
                        std::string_view duplicate_policy = "BLOCK");

    bool hset(std::string_view key, std::string_view field,
              std::string_view val);
    void AppendHset(std::string_view key, std::string_view field,
                    std::string_view val);
    void AppendTsAdd(std::string_view key_name, int64_t time, double value);

   private:
    int command_count = 0;
};

#endif
