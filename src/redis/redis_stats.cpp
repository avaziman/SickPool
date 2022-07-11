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

bool RedisManager::UpdateStats(worker_map worker_stats_map,
                               miner_map miner_stats_map,
                               int64_t update_time_ms, uint8_t update_flags)
{
    std::scoped_lock lock(rc_mutex);

    double pool_hr = 0;
    if (update_flags & UPDATE_INTERVAL)
    {
        for (const auto &[worker_name, worker_stats] : worker_stats_map)
        {
            AppendIntervalStatsUpdate(worker_name, "worker", update_time_ms,
                                      worker_stats);
        }
    }

    for (auto &[miner_addr, miner_stats] : miner_stats_map)
    {
        if (update_flags & UPDATE_INTERVAL)
        {
            AppendIntervalStatsUpdate(miner_addr, "miner", update_time_ms,
                                      miner_stats);

            AppendCommand("ZADD solver-index:hashrate %f %b",
                          miner_stats.interval_hashrate, miner_addr.data(),
                          miner_addr.size());
            pool_hr += miner_stats.interval_hashrate;
        }

        if (update_flags & UPDATE_EFFORT)
        {
            AppendSetMinerEffort(coin_symbol, miner_addr, "pow",
                                 miner_stats.round_effort_map[coin_symbol]);
        }
    }

    if (update_flags & UPDATE_INTERVAL)
    {
        AppendTsAdd("pool_hashrate", update_time_ms, pool_hr);
    }

    if (update_flags & UPDATE_EFFORT)
    {
        AppendHset(fmt::format("{}:round:pow", COIN_SYMBOL), "total_effort",
                   std::to_string(pool_hr));
    }

    return GetReplies();
}

bool RedisManager::LoadMinersAverageHashrate(miner_map &miner_stats_map)
{
    std::scoped_lock lock(rc_mutex);

    // auto now = GetCurrentTimeMs();
    // // exactly same formula as updating stats
    // int64_t from = now -
    //                (now % (StatsManager::hashrate_interval_seconds * 1000)) -
    //                (StatsManager::hashrate_interval_seconds * 1000);

    // auto miners_reply =
    //     (redisReply *)redisCommand(rc, "TS.MRANGE FILTER type=hashrate:average");

    // size_t miner_count = 0;

    // // either load everyone or no
    // {
    //     RedisTransaction load_tx(this);

    //     miner_count = miners_reply->elements;
    //     for (int i = 0; i < miner_count; i++)
    //     {
    //         std::string miner_addr(miners_reply->element[i]->str,
    //                                miners_reply->element[i]->len);

    //         // load round effort PoW
    //         AppendCommand("HGET %b:round:pow:effort %b", coin_symbol.data(),
    //                       coin_symbol.size(), miner_addr.data(),
    //                       miner_addr.size());

    //         // load sum of hashrate over last average period
    //         AppendCommand(
    //             "TS.RANGE hashrate:miner:%b %" PRIi64
    //             " + AGGREGATION SUM %" PRIi64,
    //             miner_addr.data(), miner_addr.size(), from,
    //             StatsManager::average_hashrate_interval_seconds * 1000);

    //         // reset worker count
    //         AppendCommand("ZADD solver-index:worker-count 0 %b",
    //                       miner_addr.data(), miner_addr.size());

    //         AppendTsAdd(fmt::format("worker-count:{}", miner_addr), now, 0.f);
    //     }
    // }

    // redisReply *reply;
    // for (int i = 0; i < command_count; i++)
    // {
    //     if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
    //     {
    //         Logger::Log(LogType::Critical, LogField::StatsManager,
    //                     "Failed to queue round_entries:pow get");
    //         return false;
    //     }

    //     // we need to read the last response
    //     if (i != command_count - 1)
    //     {
    //         freeReplyObject(reply);
    //     }
    // }
    // command_count = 0;

    // for (int i = 0; i < miner_count; i++)
    // {
    //     auto miner_addr = std::string(miners_reply->element[i]->str,
    //                                   miners_reply->element[i]->len);

    //     const double miner_effort = 0;
    //     auto round_effort_reply = reply->element[i * 4];
    //     if (round_effort_reply->type == REDIS_REPLY_STRING)
    //     {
    //         std::strtod(round_effort_reply->str, nullptr);
    //     }

    //     double sum_last_avg_interval = 0;
    //     auto average_sum_reply = reply->element[i * 4 + 1];
    //     if (average_sum_reply->elements &&
    //         average_sum_reply->element[0]->elements)
    //     {
    //         sum_last_avg_interval = std::strtod(
    //             average_sum_reply->element[0]->element[1]->str, nullptr);
    //     }

    //     miner_stats_map[miner_addr].round_effort_map[COIN_SYMBOL] =
    //         miner_effort;
    //     miner_stats_map[miner_addr].average_hashrate_sum +=
    //         sum_last_avg_interval;

    //     Logger::Log(LogType::Debug, LogField::StatsManager,
    //                 "Loaded {} effort of {}, hashrate sum of {}", miner_addr,
    //                 miner_effort, sum_last_avg_interval);
    // }

    // freeReplyObject(reply);
    // freeReplyObject(miners_reply);
    return true;
}

bool RedisManager::LoadMinersEfforts(const std::string &chain,
                                     miner_map &miners_map)
{
    auto reply = (redisReply *)redisCommand(rc, "HGETALL %b:round:pow:effort",
                                            chain.data(), chain.size());

    for (int i = 0; i < reply->elements; i += 2)
    {
        std::string addr(reply->element[i]->str, reply->element[i]->len);

        double effort = std::strtod(reply->element[i + 1]->str, nullptr);

        miners_map[addr].round_effort_map[chain] = effort;
    }

    freeReplyObject(reply);
    return true;
}