#include "redis_manager.hpp"

#include "payment_manager.hpp"

using enum Prefix;

const Logger<RedisManager::logger_field> RedisManager::logger;
redis_unique_ptr_context RedisManager::rc_unique;
std::mutex RedisManager::rc_mutex;

RedisManager::RedisManager(const CoinConfig &conf)
    : conf(&conf), key_names(conf.symbol)
{
    SockAddr addr(conf.redis.host);

    RedisManager::rc_unique = redis_unique_ptr_context(
        redisConnect(addr.ip_str.c_str(), addr.port_original), redisFree);

    if (rc_unique->err)
    {
        logger.Log<LogType::Critical>("Failed to connect to redis: {}",
                                      rc_unique->errstr);
        throw std::invalid_argument("Failed to connect to redis");
    }

    Command({"SELECT", std::to_string(conf.redis.db_index)});
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

        // we want x7 duration (7 days)
        AppendCommand({"TS.CREATE"sv, key_compact_name, "RETENTION"sv,
                       std::to_string(hashrate_ttl_ms * 7)});

        // we want 12 times less points to fit in 7 days (1 hr instead of 5min)
        AppendCommand({"TS.CREATERULE"sv, key_name, key_compact_name,
                       "ROLL_AGGREGATION"sv, "roll_avg"sv, STRR(12)});
    }


    if (!GetReplies())
    {
        logger.Log<LogType::Critical>(
            "Failed to connect to add pool timeserieses", rc_unique->errstr);
        throw std::invalid_argument(
            "Failed to connect to add pool timeserieses");
    }
}

void RedisManager::AppendTsAdd(std::string_view key_name, int64_t time,
                               double value)
{
    using namespace std::string_view_literals;
    AppendCommand(
        {"TS.ADD"sv, key_name, std::to_string(time), std::to_string(value)});
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

double RedisManager::zscore(std::string_view key, std::string_view field)
{
    std::scoped_lock _(rc_mutex);

    return ResToDouble(Command({"ZSCORE", key, field}));
}
