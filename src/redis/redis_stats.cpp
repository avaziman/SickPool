#include "redis_manager.hpp"

void RedisManager::AppendIntervalStatsUpdate(std::string_view addr,
                                             std::string_view prefix,
                                             int64_t update_time_ms,
                                             const WorkerStats &ws)
{
    // miner hashrate
    std::string prefix_addr = fmt::format("{}:{}", prefix, addr);
    std::string key = fmt::format("hashrate:{}", prefix_addr);
    AppendTsAdd(key, update_time_ms, ws.interval_hashrate);

    // average hashrate
    key = fmt::format("hashrate:average:{}", prefix_addr);
    AppendTsAdd(key, update_time_ms, ws.average_hashrate);

    // shares
    key = fmt::format("shares:valid:{}", prefix_addr);
    AppendTsAdd(key, update_time_ms, ws.interval_valid_shares);

    key = fmt::format("shares:invalid:{}", prefix_addr);
    AppendTsAdd(key, update_time_ms, ws.interval_invalid_shares);

    key = fmt::format("shares:stale:{}", prefix_addr);
    AppendTsAdd(key, update_time_ms, ws.interval_stale_shares);
}

bool RedisManager::UpdateEffortStats(efforts_map_t &miner_stats_map,
                                     const double total_effort,
                                     std::mutex *stats_mutex)
{
    std::scoped_lock redis_lock(rc_mutex);

    {
        std::scoped_lock stats_lock(*stats_mutex);
        for (auto &[miner_addr, miner_effort] : miner_stats_map)
        {
            AppendSetMinerEffort(COIN_SYMBOL, miner_addr, "pow",
                                 miner_effort);
        }

        AppendSetMinerEffort(COIN_SYMBOL, TOTAL_EFFORT_KEY, "pow",
                             total_effort);
    }

    return GetReplies();
}

bool RedisManager::UpdateIntervalStats(worker_map &worker_stats_map,
                                       worker_map &miner_stats_map,
                                       std::mutex *stats_mutex,
                                       int64_t update_time_ms)
{
    Benchmark<std::chrono::microseconds> bench("UPDATE SATAS REDIS");
    std::scoped_lock lock(rc_mutex);

    double pool_hr = 0;
    {
        // don't lock stats_mutex when awaiting replies as it's slow
        // unnecessary
        std::scoped_lock stats_lock(*stats_mutex);

        for (auto &[worker_name, worker_stats] : worker_stats_map)
        {
            AppendIntervalStatsUpdate(worker_name, "worker", update_time_ms,
                                      worker_stats);
            worker_stats.ResetInterval();
            pool_hr += worker_stats.interval_hashrate;
        }

        for (auto &[miner_addr, miner_stats] : miner_stats_map)
        {
            AppendIntervalStatsUpdate(miner_addr, "miner", update_time_ms,
                                      miner_stats);

            AppendCommand("ZADD solver-index:hashrate %f %b",
                          miner_stats.interval_hashrate, miner_addr.data(),
                          miner_addr.size());
            miner_stats.ResetInterval();
        }
    }

    AppendTsAdd("pool_hashrate", update_time_ms, pool_hr);

    return GetReplies();
}

bool RedisManager::LoadAverageHashrateSum(
    std::vector<std::pair<std::string, double>> &hashrate_sums,
    std::string_view prefix)
{
    int64_t time_now = GetCurrentTimeMs();
    int64_t to = time_now - time_now % StatsManager::average_hashrate_interval_seconds * 1000 +
                   StatsManager::average_hashrate_interval_seconds * 1000;

    int64_t from = to - StatsManager::average_hashrate_interval_seconds * 1000;
    TsAggregation aggregation{
        .type = "SUM",
        .time_bucket_ms =
            StatsManager::average_hashrate_interval_seconds * 1000};
    return TsMrange(hashrate_sums, prefix, "hashrate", from, to, &aggregation);
}


bool RedisManager::ResetMinersWorkerCounts(efforts_map_t &miner_stats_map,
                                           int64_t time_now)
{
    for (auto &[addr, _] : miner_stats_map)
    {
        // reset worker count
        AppendCommand("ZADD solver-index:worker-count 0 %b", addr.data(),
                      addr.size());

        AppendTsAdd(fmt::format("worker-count:{}", addr), time_now, 0.f);
    }

    return GetReplies();
}

bool RedisManager::LoadMinersEfforts(
    std::string_view chain, std::string_view type,
    efforts_map_t &efforts)
{
    auto reply = (redisReply *)redisCommand(rc, "HGETALL %b:round:%b:effort",
                                            chain.data(), chain.size(), type.data(), type.size());

    for (int i = 0; i < reply->elements; i += 2)
    {
        std::string addr(reply->element[i]->str, reply->element[i]->len);

        double effort = std::strtod(reply->element[i + 1]->str, nullptr);

        efforts[addr] = effort;
    }

    freeReplyObject(reply);
    return true;
}

bool RedisManager::TsMrange(
    std::vector<std::pair<std::string, double>> &last_averages,
    std::string_view prefix, std::string_view type, int64_t from, int64_t to,
    TsAggregation *aggregation)
{
    std::scoped_lock locl(rc_mutex);

    std::string cmd_str;
    if (aggregation)
    {
        cmd_str = fmt::format(
            "TS.MRANGE {} {} AGGREGATION {} {} FILTER prefix={} type={}", from,
            to, aggregation->type, aggregation->time_bucket_ms, prefix, type);
    }
    else
    {
        cmd_str = fmt::format("TS.MRANGE {} {} FILTER prefix={} type={}", from,
                              to, prefix, type);
    }

    auto reply = (redisReply *)redisCommand(rc, cmd_str.c_str());

    if (!reply) return false;

    // we don't want empty elements
    // last_averages.resize(reply->elements);
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

        std::string addr(
            addr_start,
            (entry->element[0]->str + entry->element[0]->len) - addr_start);

        double hashrate = std::strtod(
            entry->element[2]->element[0]->element[1]->str, nullptr);
        // last_averages[i] = std::make_pair(addr, hashrate);
        last_averages.emplace_back(addr, hashrate);
    }

    return true;
}