#include "redis_manager.hpp"

#include "payout_manager.hpp"

using enum Prefix;
using namespace sw::redis;
using namespace std::string_view_literals;

const Logger RedisManager::logger{RedisManager::logger_field};
std::mutex RedisManager::rc_mutex{};
std::unique_ptr<Redis> RedisManager::redis{};

RedisManager::RedisManager(const CoinConfig &conf)
    : conf(&conf), key_names(conf.symbol)
{
    SockAddr addr(conf.redis.host);
    ConnectionOptions connection_options;
    connection_options.host = addr.ip_str;  // Required.
    connection_options.port =
        addr.port_original;  // Optional. The default port is 6379.
    // connection_options.socket_timeout = std::chrono::milliseconds(200);

    ConnectionPoolOptions pool_options;
    pool_options.size = 3;  // Pool size, i.e. max number of connections.

    RedisManager::redis = std::make_unique<Redis>(connection_options, pool_options);

    // if (rc_unique->err)
    // {
    //     logger.Log<LogType::Critical>("Failed to connect to redis: {}",
    //                                   rc_unique->errstr);
    //     throw std::invalid_argument("Failed to connect to redis");
    // }

   redis->command("SELECT", conf.redis.db_index);
}
void RedisManager::Init()
{
    using namespace std::string_view_literals;

    const uint64_t hashrate_ttl_ms = conf->redis.hashrate_ttl_seconds * 1000;
    auto pipe =redis->pipeline(false);

    pipe.command("TS.CREATE"sv, key_names.hashrate_pool, "RETENTION"sv,
                 std::to_string(hashrate_ttl_ms));

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
        TimeSeries ts{hashrate_ttl_ms, {}, DuplicatePolicy::BLOCK};
        AppendTsCreate(pipe, key_name, ts);

        // x7 duration (7 days)
        TimeSeries ts_compact{hashrate_ttl_ms * 7, {}, DuplicatePolicy::BLOCK};
        AppendTsCreate(pipe, key_name, ts_compact);

        // we want 12 times less points to fit in 7 days (2 hr instead of 10min)
        pipe.command("TS.CREATERULE"sv, key_name, key_compact_name,
                     "ROLL_AGGREGATION"sv, "roll_avg"sv, STRR(12));
    }

    pipe.exec();
    // if (!GetReplies())
    // {
    //     logger.Log<LogType::Critical>(
    //         "Failed to connect to add pool timeserieses", rc_unique->errstr);
    //     throw std::invalid_argument(
    //         "Failed to connect to add pool timeserieses");
    // }
}

void RedisManager::AppendTsAdd(
    sw::redis::Pipeline& pipe, std::string_view key_name, int64_t time,
    double value)
{
    pipe.command("TS.ADD", key_name, std::to_string(time),
                  std::to_string(value));
}

void RedisManager::AppendTsCreate(sw::redis::Pipeline& pipe, std::string_view key, const TimeSeries& ts)
{

    // EnumName<ts.duplicate_policy>()
    auto params = ts.GetCreateParams(key);
    pipe.command(params.begin(), params.end());
}