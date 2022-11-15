#include "redis_manager.hpp"

#include "payment_manager.hpp"

using enum Prefix;

const Logger<RedisManager::logger_field> RedisManager::logger;
redis_unique_ptr_context RedisManager::rc_unique;
redisContext *RedisManager::rc;
std::mutex RedisManager::rc_mutex;

RedisManager::RedisManager(const std::string &ip, const CoinConfig *conf,
                           int db_index)
    : conf(conf), key_names(conf->symbol)
{
    RedisManager::rc_unique = redis_unique_ptr_context(
        redisConnect(ip.c_str(), conf->redis.redis_port), redisFree);
    RedisManager::rc = rc_unique.get();

    if (rc->err)
    {
        logger.Log<LogType::Critical>("Failed to connect to redis: {}",
                                      rc->errstr);
        // redisFree(rc);
        throw std::runtime_error("Failed to connect to redis");
    }

    Command({"SELECT", std::to_string(db_index)});
}
void RedisManager::Init()
{
    using namespace std::string_view_literals;

    const int64_t hashrate_ttl_ms = conf->redis.hashrate_ttl_seconds * 1000;
    const int64_t hashrate_interval_ms =
        conf->stats.hashrate_interval_seconds * 1000;

    AppendCommand({"TS.CREATE"sv, key_names.hashrate_pool, "RETENTION"sv,
                   std::to_string(hashrate_ttl_ms)});

    for (const auto &[key_name, key_compact_name] :
         {std::make_pair(key_names.hashrate_pool,
                         key_names.hashrate_pool_compact),
          std::make_pair(key_names.hashrate_network,
                         key_names.hashrate_network_compact),
          std::make_pair(key_names.worker_count_pool,
                         key_names.worker_countp_compact),
          std::make_pair(key_names.miner_count, key_names.miner_count_compact),
          std::make_pair(key_names.difficulty, key_names.difficulty_compact)})
    {
        AppendCommand({"TS.CREATE"sv, key_name, "RETENTION"sv,
                       std::to_string(hashrate_ttl_ms)});

        // we want x7 duration
        AppendCommand({"TS.CREATE"sv, key_compact_name, "RETENTION"sv,
                       std::to_string(hashrate_ttl_ms * 7)});

        // we want 12 times less points to fit in 7 days (1 hr instead of 5min)
        AppendCommand({"TS.CREATERULE"sv, key_name, key_compact_name,
                       "AGGREGATION"sv, "AVG"sv,
                       std::to_string(hashrate_interval_ms * 12)});
    }

    // mined blocks, only compact needed.
    AppendCommand({"TS.CREATE"sv, key_names.block_number_compact, "RETENTION"sv,
                   std::to_string(hashrate_ttl_ms * 7)});

    // effort percent
    auto key_name = key_names.block_effort_percent;
    auto key_compact_name = key_names.block_effort_percent_compact;
    AppendCommand({"TS.CREATE"sv, key_name, "RETENTION"sv,
                   std::to_string(hashrate_ttl_ms * 7)});

    AppendCommand({"TS.CREATE"sv, key_compact_name, "RETENTION"sv,
                   std::to_string(hashrate_ttl_ms * 7)});

    AppendCommand({"TS.CREATERULE"sv, key_name, key_compact_name,
                   "AGGREGATION"sv, "AVG"sv,
                   std::to_string(hashrate_interval_ms * 12)});

    if (!GetReplies())
    {
        logger.Log<LogType::Critical>(
            "Failed to connect to add pool timeserieses", rc->errstr);
        throw std::runtime_error("Failed to connect to add pool timeserieses");
    }
}

RedisManager::~RedisManager() {}

bool RedisManager::SetActiveId(const MinerIdHex &id)
{
    std::scoped_lock lock(rc_mutex);
    // remove pending removal if there is
    AppendCommand(
        {"SREM", key_names.active_ids_map, fmt::format("-{}", id.GetHex())});

    AppendCommand({"SADD", key_names.active_ids_map, id.GetHex()});

    return GetReplies();
}
// bool RedisManager::SetNewBlockStats(std::string_view chain, int64_t
// curtime_ms,
//                                     double net_est_hr, double target_diff)
// {
//     std::scoped_lock lock(rc_mutex);

//     // AppendTsAdd(key_name.hashrate_network, curtime_ms, net_est_hr);

//     // AppendSetMinerEffort(chain, EnumName<ESTIMATED_EFFORT>(), "pow",
//     //                      target_diff);
//     return GetReplies();
// }

bool RedisManager::UpdateImmatureRewards(uint8_t chain, uint32_t block_num,
                                         int64_t matured_time, bool matured)
{
    using namespace std::string_view_literals;
    using namespace std::string_literals;
    std::scoped_lock lock(rc_mutex);

    auto reply = Command({"HGETALL"sv, Format({key_names.reward_immature,
                                               std::to_string(block_num)})});

    int64_t matured_funds = 0;
    // either mature everything or nothing
    {
        RedisTransaction update_rewards_tx(this);

        for (int i = 0; i < reply->elements; i += 2)
        {
            std::string_view addr(reply->element[i]->str,
                                  reply->element[i]->len);

            RoundReward *miner_share =
                (RoundReward *)reply->element[i + 1]->str;

            // if a block has been orphaned only remove the immature
            // if there are sub chains add chain:

            if (!matured)
            {
                miner_share->reward = 0;
            }

            // used to show round share statistics
            uint8_t
                round_share_block_num[sizeof(miner_share) + sizeof(block_num)];
            memcpy(round_share_block_num, miner_share, sizeof(miner_share));
            memcpy(round_share_block_num + sizeof(miner_share), &block_num,
                   sizeof(block_num));

            std::string mature_rewards_key =
                Format({key_names.reward_mature, addr});
            AppendCommand({"LPUSH"sv, mature_rewards_key,
                           std::string_view((char *)round_share_block_num,
                                            sizeof(round_share_block_num))});

            AppendCommand({"LTRIM"sv, mature_rewards_key, "0"sv,
                           std::to_string(ROUND_SHARES_LIMIT)});

            std::string reward_str = std::to_string(miner_share->reward);
            std::string_view reward_sv(reward_str);

            // // used for payments
            // AppendCommand({"HSET"sv, fmt::format("mature-rewards:{}", addr),
            //                std::to_string(block_num), reward_sv});

            AppendCommand(
                {"ZINCRBY"sv, key_names.solver_index_mature, reward_sv, addr});

            std::string_view solver_key =
                fmt::format("{}:{}", key_names.solver, addr);

            AppendCommand({"HINCRBY"sv, solver_key, EnumName<MATURE_BALANCE>(),
                           reward_sv});
            AppendCommand({"HINCRBY"sv, solver_key,
                           EnumName<IMMATURE_BALANCE>(),
                           fmt::format("-{}", reward_sv)});

            // for payment manager...
            AppendCommand({"PUBLISH", key_names.block_mature_channel, "OK"});

            matured_funds += miner_share->reward;
        }

        // we pushed it to mature round shares list
        AppendCommand({"UNLINK"sv, Format({key_names.reward_immature,
                                           std::to_string(block_num)})});
    }

    logger.Log<LogType::Info>("{} funds have matured!", matured_funds);
    return GetReplies();
}

bool RedisManager::LoadUnpaidRewards(payees_info_t &rewards,
                                     const std::vector<MinerIdHex>& active_ids)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction tx(this);

        rewards.reserve(active_ids.size());

        for (auto& id : active_ids)
        {
            AppendCommand(
                {"HMGET"sv, Format({key_names.solver, id.GetHex()}),
                 EnumName<ADDRESS>(), EnumName<MATURE_BALANCE>(),
                 EnumName<PAYOUT_THRESHOLD>(), EnumName<PAYOUT_FEELESS>()});
            // rewards.emplace_back(addr, 0);
        }
    }

    redis_unique_ptr reply;
    if (!GetReplies(&reply)) return false;

    for (MinerId i = 0; i < reply->elements; i++)
    {
        if (reply->element[i]->elements != 4 ||
            reply->element[i]->element[0]->type != REDIS_REPLY_STRING ||
            reply->element[i]->element[1]->type != REDIS_REPLY_STRING ||
            reply->element[i]->element[2]->type != REDIS_REPLY_STRING ||
            reply->element[i]->element[3]->type != REDIS_REPLY_STRING)
        {
            continue;
        }
        auto addr_rep = reply->element[i]->element[0];
        rewards[i].first = std::string(addr_rep->str, addr_rep->len);

        rewards[i].second = PayeeInfo{
            .amount =
                std::strtoll(reply->element[i]->element[1]->str, nullptr, 10),
            .settings = {.threshold = std::strtoll(
                             reply->element[i]->element[2]->str, nullptr, 10),
                         .pool_block_only =
                             reply->element[i]->element[3]->str[0] == '1'}};
        // const auto script_reply = reply->element[i]->element[2];
        // const auto script_len = script_reply->len;

        // rewards[i].second.script_pub_key.resize(script_len / 2);
        // Unhexlify(rewards[i].second.script_pub_key.data(), script_reply->str,
        //           script_len);
    }
    return true;
}

void RedisManager::AppendTsAdd(std::string_view key_name, int64_t time,
                               double value)
{
    using namespace std::string_view_literals;
    AppendCommand(
        {"TS.ADD"sv, key_name, std::to_string(time), fmt::format("{}", value)});
}

void RedisManager::AppendTsCreate(std::string_view key, std::string_view prefix,
                                  std::string_view type,
                                  std::string_view address, std::string_view id,
                                  uint64_t retention_ms)
{
    using namespace std::string_view_literals;
    AppendCommand({"TS.CREATE"sv, key, "RETENTION"sv,
                   std::to_string(retention_ms), "LABELS"sv, "type"sv, type,
                   "prefix"sv, prefix, "address"sv, address, "id"sv, id});
}

bool RedisManager::GetActiveIds(std::vector<MinerIdHex> &addresses)
{
    std::scoped_lock locl(rc_mutex);

    auto cmd = Command({"SMEMBERS", key_names.active_ids_map});

    addresses.reserve(cmd->elements);
    for (int i = 0; i < cmd->elements; i++)
    {
        std::string key(cmd->element[i]->str, cmd->element[i]->str);

        uint32_t id;
        std::from_chars(cmd->str, cmd->str + cmd->len, id, 16);

        addresses.push_back(MinerIdHex(id));
    }
    return cmd.get();
}
// bool RedisManager::PopWorker(std::string_view address)
// {
//     std::scoped_lock lock(rc_mutex);

//     AppendUpdateWorkerCount(address, -1);

//     return GetReplies();
// }

void RedisManager::AppendHset(std::string_view key, std::string_view field,
                              std::string_view val)
{
    AppendCommand({"HSET", key, field, val});
}

bool RedisManager::hset(std::string_view key, std::string_view field,
                        std::string_view val)
{
    AppendHset(key, field, val);
    return GetReplies();
}

std::string RedisManager::hget(std::string_view key, std::string_view field)
{
    using namespace std::string_view_literals;
    auto reply = Command({"HGET"sv, key, field});

    if (reply && reply->type == REDIS_REPLY_STRING)
    {
        // returns 0 if failed
        return std::string(reply->str, reply->len);
    }
    return std::string();
}

bool RedisManager::TsMrange(
    std::vector<std::pair<WorkerFullId, double>> &last_averages,
    std::string_view prefix, std::string_view type, int64_t from, int64_t to,
    const TsAggregation *aggregation)
{
    using namespace std::string_view_literals;
    std::scoped_lock locl(rc_mutex);

    redis_unique_ptr reply;
    if (aggregation)
    {
        reply = Command({"TS.MRANGE"sv, std::to_string(from),
                         std::to_string(to), "AGGREGATION"sv, aggregation->type,
                         std::to_string(aggregation->time_bucket_ms),
                         "FILTER"sv, fmt::format("prefix={}", prefix),
                         fmt::format("type={}", type)});
    }
    else
    {
        reply =
            Command({"TS.MRANGE"sv, std::to_string(from), std::to_string(to),
                     "FILTER"sv, fmt::format("prefix={}", prefix),
                     fmt::format("type={}", type)});
    }

    if (!reply.get()) return false;

    last_averages.reserve(reply->elements);

    for (int i = 0; i < reply->elements; i++)
    {
        auto entry = reply->element[i];

        // everything that can go wrong with the reply
        if (entry->type != REDIS_REPLY_ARRAY || entry->elements < 3 ||
            !entry->element[2]->elements ||
            entry->element[2]->element[0]->type != REDIS_REPLY_ARRAY)
        {
            continue;
        }

        char *addr_start = std::strrchr(entry->element[0]->str, ':');

        if (addr_start == nullptr)
        {
            continue;
        }

        addr_start++;  // skip ':'

        std::string id_hex(
            addr_start,
            (entry->element[0]->str + entry->element[0]->len) - addr_start);

        MinerId miner_id;
        WorkerId worker_id;

        std::to_chars(id_hex.data(), id_hex.data() + sizeof(miner_id) * 2,
                      miner_id);
        std::to_chars(
            id_hex.data() + sizeof(miner_id) * 2,
            id_hex.data() + sizeof(miner_id) * 2 + sizeof(worker_id) * 2,
            worker_id);

        double hashrate = std::strtod(
            entry->element[2]->element[0]->element[1]->str, nullptr);
        // last_averages[i] = std::make_pair(addr, hashrate);
        last_averages.emplace_back(WorkerFullId(miner_id, worker_id), hashrate);
    }

    return true;
}

bool RedisManager::UpdateBlockNumber(int64_t time, uint32_t number)
{
    std::scoped_lock lock(rc_mutex);
    AppendTsAdd(key_names.block_number_compact, time, number);
    return GetReplies();
}