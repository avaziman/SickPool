#include "redis_manager.hpp"

#include "payment_manager.hpp"

using enum Prefix;

const Logger<RedisManager::logger_field> RedisManager::logger;
redis_unique_ptr_context RedisManager::rc_unique;
std::mutex RedisManager::rc_mutex;

RedisManager::RedisManager(const std::string &ip, const CoinConfig *conf,
                           int db_index)
    : conf(conf), key_names(conf->symbol)
{
    RedisManager::rc_unique = redis_unique_ptr_context(
        redisConnect(ip.c_str(), conf->redis.redis_port), redisFree);

    if (rc_unique->err)
    {
        logger.Log<LogType::Critical>("Failed to connect to redis: {}",
                                      rc_unique->errstr);
        throw std::invalid_argument("Failed to connect to redis");
    }

    Command({"SELECT", std::to_string(db_index)});
}
void RedisManager::Init()
{
    using namespace std::string_view_literals;

    const int64_t hashrate_ttl_ms = conf->redis.hashrate_ttl_seconds * 1000;
    const int64_t mined_blocks_interval =
        conf->stats.mined_blocks_interval * 1000;
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

        // we want x7 duration (7 days)
        AppendCommand({"TS.CREATE"sv, key_compact_name, "RETENTION"sv,
                       std::to_string(hashrate_ttl_ms * 7)});

        // we want 12 times less points to fit in 7 days (1 hr instead of 5min)
        AppendCommand({"TS.CREATERULE"sv, key_name, key_compact_name,
                       "ROLL_AGGREGATION"sv, "roll_avg"sv, STRR(12)});
    }

    // mined blocks, only compact needed.
    // AppendCommand({"TS.CREATE"sv, key_names.mined_block_number, "RETENTION"sv,
    //                std::to_string(mined_blocks_interval)});

    // effort percent
    // auto key_name = key_names.block_effort_percent;
    // auto key_compact_name = key_names.block_effort_percent_compact;
    // AppendCommand({"TS.CREATE"sv, key_name, "RETENTION"sv,
    //                std::to_string(hashrate_ttl_ms * 7)});

    // AppendCommand({"TS.CREATE"sv, key_compact_name, "RETENTION"sv,
    //                std::to_string(hashrate_ttl_ms * 7)});

    // AppendCommand({"TS.CREATERULE"sv, key_name, key_compact_name,
    //                "AGGREGATION"sv, "AVG"sv,
    //                std::to_string(hashrate_interval_ms * 12)});

    if (!GetReplies())
    {
        logger.Log<LogType::Critical>(
            "Failed to connect to add pool timeserieses", rc_unique->errstr);
        throw std::invalid_argument(
            "Failed to connect to add pool timeserieses");
    }
}

bool RedisManager::SetActiveId(const MinerIdHex &id)
{
    std::scoped_lock lock(rc_mutex);
    // remove pending removal if there is
    AppendCommand(
        {"SREM", key_names.active_ids_map, fmt::format("-{}", id.GetHex())});

    AppendCommand({"SADD", key_names.active_ids_map, id.GetHex()});

    return GetReplies();
}

bool RedisManager::LoadUnpaidRewards(payees_info_t &rewards,
                                     const std::vector<MinerIdHex> &active_ids)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction tx(this);

        rewards.reserve(active_ids.size());

        for (auto &id : active_ids)
        {
            AppendCommand({"HMGET"sv, Format({key_names.solver, id.GetHex()}),
                           EnumName<ADDRESS>(), EnumName<MATURE_BALANCE>(),
                           EnumName<PAYOUT_THRESHOLD>(),
                           EnumName<PAYOUT_FEELESS>()});
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

void RedisManager::AppendTsCreateMiner(std::string_view key,
                                       std::string_view type,
                                       std::string_view address,
                                       std::string_view id,
                                       uint64_t retention_ms,
                                       std::string_view duplicate_policy)
{
    // cant have empty labels
    if (id == "") id = "null";

    using namespace std::string_view_literals;
    AppendCommand({"TS.CREATE"sv, key, "RETENTION"sv,
                   std::to_string(retention_ms), "DUPLICATE_POLICY"sv,
                   duplicate_policy, "LABELS"sv, "type"sv, type, "prefix"sv,
                   EnumName<Prefix::MINER>(), "address"sv, address, "id"sv,
                   id});
}

void RedisManager::AppendTsCreateWorker(
    std::string_view key, std::string_view type, std::string_view address,
    std::string_view id, uint64_t retention_ms, std::string_view worker_name,
    std::string_view duplicate_policy)
{
    // cant have empty labels
    if (id == "") id = "null";

    using namespace std::string_view_literals;
    AppendCommand({"TS.CREATE"sv, key, "RETENTION"sv,
                   std::to_string(retention_ms), "DUPLICATE_POLICY"sv,
                   duplicate_policy, "LABELS"sv, "type"sv, type, "prefix"sv,
                   EnumName<Prefix::WORKER>(), "address"sv, address, "id"sv, id,
                   "worker_name", worker_name});
}

bool RedisManager::GetActiveIds(std::vector<MinerIdHex> &addresses)
{
    std::scoped_lock locl(rc_mutex);

    auto cmd = Command({"SMEMBERS", key_names.active_ids_map});

    addresses.reserve(cmd->elements);
    for (int i = 0; i < cmd->elements; i++)
    {
        std::string key(cmd->element[i]->str, cmd->element[i]->len);

        uint32_t id;
        std::from_chars(key.data(), key.data() + key.size(), id, 16);

        addresses.push_back(MinerIdHex(id));
    }
    return cmd.get();
}

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

    if (auto reply = Command({"HGET"sv, key, field});
        reply && reply->type == REDIS_REPLY_STRING)
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
        last_averages.emplace_back(WorkerFullId(miner_id, worker_id), hashrate);
    }

    return true;
}