#include "redis_stats.hpp"

using enum Prefix;

void RedisStats::AppendIntervalStatsUpdate(std::string_view addr,
                                           std::string_view prefix,
                                           int64_t update_time_ms,
                                           const WorkerStats &ws)
{
    // miner hashrate
    std::string prefix_addr = Format({prefix, addr});
    std::string key = Format({key_names.hashrate, prefix_addr});
    AppendTsAdd(key, update_time_ms, ws.interval_hashrate);

    // shares
    key = Format({key_names.shares_valid, prefix_addr});
    AppendTsAdd(key, update_time_ms, ws.interval_valid_shares);

    key = Format({key_names.shares_invalid, prefix_addr});
    AppendTsAdd(key, update_time_ms, ws.interval_invalid_shares);

    key = Format({key_names.shares_stale, prefix_addr});
    AppendTsAdd(key, update_time_ms, ws.interval_stale_shares);
}

bool RedisStats::UpdateIntervalStats(
    worker_map &worker_stats_map, miner_map &miner_stats_map,
    std::unique_lock<std::shared_mutex> stats_mutex, const NetworkStats& ns, int64_t update_time_ms)
{
    using namespace std::string_view_literals;
    std::scoped_lock lock(rc_mutex);

    double pool_hr = 0;
    uint32_t pool_worker_count = 0;
    uint32_t pool_miner_count = 0;

    // don't lock stats_mutex when awaiting replies as it's slow
    // unnecessary
    for (auto &[worker_id, worker_stats] : worker_stats_map)
    {
        AppendIntervalStatsUpdate(std::to_string(worker_id.worker_id), EnumName<WORKER>(),
                                  update_time_ms, worker_stats);
        if (worker_stats.interval_hashrate > 0) pool_worker_count++;

        worker_stats.ResetInterval();
        pool_hr += worker_stats.interval_hashrate;
    }

    for (auto &[miner_id, miner_stats] : miner_stats_map)
    {
        std::string hr_str(std::to_string(miner_stats.interval_hashrate));
        std::string miner_id_str(std::to_string(miner_id));

        AppendIntervalStatsUpdate(miner_id_str, EnumName<MINER>(),
                                  update_time_ms, miner_stats);

        AppendCommand({"ZADD"sv, key_names.miner_index_hashrate, hr_str,
                       std::to_string(miner_id)});

        AppendUpdateWorkerCount(miner_id, miner_stats.worker_count,
                                update_time_ms);

        if (miner_stats.interval_hashrate > 0) pool_miner_count++;

        miner_stats.ResetInterval();
    }
    stats_mutex.unlock();

    // net hr
    AppendTsAdd(key_names.hashrate_network, update_time_ms, ns.network_hr);

    // diff
    AppendTsAdd(key_names.difficulty, update_time_ms, ns.difficulty);

    // pool hr, workers, miners
    AppendTsAdd(key_names.hashrate_pool, update_time_ms, pool_hr);
    AppendTsAdd(key_names.worker_count_pool, update_time_ms, pool_miner_count);
    AppendTsAdd(key_names.miner_count, update_time_ms, pool_worker_count);

    return GetReplies();
}

// bool RedisStats::ResetMinersWorkerCounts(
//     const std::vector<MinerId> &addresses, int64_t time_now)
// {
//     using namespace std::string_view_literals;
//     for (const auto &[id, _] : addresses)
//     {
//         // reset worker count
//         AppendUpdateWorkerCount(id, 0, time_now);
//     }

//     return GetReplies();
// }

bool RedisStats::AddNewMiner(std::string_view address,
                             std::string_view addr_lowercase,
                             std::string_view alias, MinerId id,
                             int64_t curime_ms, int64_t min_payout)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction add_worker_tx(this);

        auto chain = key_names.coin;

        std::string id_str = std::to_string(id);

        std::string curime_ms_str = std::to_string(curime_ms / 1000);

        // reset all indexes of new miner
        AppendCommand({"ZADD"sv, key_names.miner_index_round_effort, "0", id_str});
        AppendCommand({"ZADD"sv, key_names.miner_index_hashrate, "0", id_str});

        // to be accessible by lowercase addr
        AppendCreateStatsTsMiner(id_str, alias, addr_lowercase, curime_ms);
    }

    return GetReplies();
}

bool RedisStats::AddNewWorker(FullId full_id,
                              std::string_view address_lowercase,
                              std::string_view worker_name,
                              std::string_view alias, uint64_t curtime_ms)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction add_worker_tx(this);

        AppendCreateStatsTsWorker(std::to_string(full_id.worker_id), alias, address_lowercase,
                                  worker_name, curtime_ms);
    }

    return GetReplies();
}

void RedisStats::AppendUpdateWorkerCount(MinerId miner_id, int amount,
                                         int64_t update_time_ms) 
{
    using namespace std::string_view_literals;
    std::string amount_str = std::to_string(amount);

    AppendCommand({"TS.ADD"sv,
                   Format({key_names.worker_count, std::to_string(miner_id)}),
                   std::to_string(update_time_ms), amount_str});
}

void RedisStats::AppendCreateStatsTsMiner(std::string_view addr,
                                     std::string_view id,
                                     std::string_view addr_lowercase_sv,
                                     uint64_t curtime_ms)
{
    using namespace std::literals;

    auto retention = conf->redis.hashrate_ttl_seconds * 1000;
    const size_t coin_sep = key_names.coin.size() + 1;
    constexpr auto prefix = EnumName<Prefix::MINER>();

    for (std::string_view key_type :
         {key_names.hashrate, key_names.hashrate_average,
          key_names.shares_valid, key_names.shares_stale,
          key_names.shares_invalid, key_names.worker_count})
    {
        auto key = Format({key_type, prefix, addr});
        key_type = key_type.substr(coin_sep);
        AppendTsCreateMiner(key, key_type, addr_lowercase_sv, id, retention,
                       "SUM"sv);
    }

    // reset worker count
    // AppendTsAdd(worker_count_key, curime_ms, 0);

    auto hr_key = Format({key_names.hashrate, prefix, addr});
    auto hr_avg_key =
        Format({key_names.hashrate_average, prefix, addr});
    AppendCommand({"TS.CREATERULE", hr_key, hr_avg_key, "ROLL_AGGREGATION",
                   "roll_avg", average_hashrate_ratio_str});

    // uint64_t hr_interval_ms = conf->stats.hashrate_interval_seconds * 1000;
    // uint64_t prev_update = curtime_ms - (curtime_ms % hr_interval_ms);

    // to have accurate rolling statistics
    // const auto ratio = conf->stats.average_hashrate_interval_seconds /
    //                    conf->stats.hashrate_interval_seconds;
    // for (int i = 0; i < ratio; i++)
    // {
    //     AppendTsAdd(hr_key, prev_update - (ratio - i - 1) * hr_interval_ms, 0);
    // }
}

void RedisStats::AppendCreateStatsTsWorker(std::string_view addr,
                                           std::string_view id,
                                           std::string_view addr_lowercase_sv,
                                           std::string_view worker_name,
                                           uint64_t curtime_ms)
{
    using namespace std::literals;

    auto retention = conf->redis.hashrate_ttl_seconds * 1000;
    const size_t coin_sep = key_names.coin.size() + 1;
    constexpr auto prefix = EnumName<Prefix::WORKER>();

    for (std::string_view key_type :
         {key_names.hashrate, key_names.hashrate_average,
          key_names.shares_valid, key_names.shares_stale,
          key_names.shares_invalid, key_names.worker_count})
    {
        auto key = Format({key_type, prefix, addr});
        key_type = key_type.substr(coin_sep);
        AppendTsCreateWorker(key, key_type, addr_lowercase_sv, id, retention, worker_name,
                            "SUM"sv);
    }

    auto hr_key = Format({key_names.hashrate, prefix, addr});
    auto hr_avg_key = Format({key_names.hashrate_average, prefix, addr});
    AppendCommand({"TS.CREATERULE", hr_key, hr_avg_key, "ROLL_AGGREGATION",
                   "roll_avg", average_hashrate_ratio_str});
}

bool RedisStats::PopWorker(WorkerId fullid)
{
    std::scoped_lock lock(rc_mutex);

    // set pending inactive, (set inactive after payout check...)
    // AppendCommand({"SREM", key_names.active_ids_map, fullid.miner_id.GetHex()});
    // AppendCommand({"SADD", key_names.active_ids_map,
    //                fmt::format("-{}", fullid.miner_id.GetHex())});

    return GetReplies();
}