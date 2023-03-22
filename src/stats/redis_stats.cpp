#include "redis_stats.hpp"

using enum Prefix;

void RedisStats::AppendIntervalStatsUpdate(sw::redis::Pipeline &pipe,
                                           std::string_view addr,
                                           std::string_view prefix,
                                           int64_t update_time_ms,
                                           const WorkerStats &ws)
{
    // miner hashrate
    std::string prefix_addr = Format({prefix, addr});
    std::string key = Format({key_names.hashrate, prefix_addr});
    AppendTsAdd(pipe, key, update_time_ms, ws.interval_hashrate);

    // shares
    key = Format({key_names.shares_valid, prefix_addr});
    AppendTsAdd(pipe, key, update_time_ms, ws.interval_valid_shares);

    key = Format({key_names.shares_invalid, prefix_addr});
    AppendTsAdd(pipe, key, update_time_ms, ws.interval_invalid_shares);

    key = Format({key_names.shares_stale, prefix_addr});
    AppendTsAdd(pipe, key, update_time_ms, ws.interval_stale_shares);
}

bool RedisStats::UpdateIntervalStats(
    worker_map &worker_stats_map, miner_map &miner_stats_map,
    std::unique_lock<std::shared_mutex> stats_mutex, const NetworkStats &ns,
    int64_t update_time_ms)
{
    using namespace std::string_view_literals;
    std::scoped_lock lock(rc_mutex);

    double pool_hr = 0;
    uint32_t pool_worker_count = 0;
    uint32_t pool_miner_count = 0;

    auto pipe =redis->pipeline(false);
    // don't lock stats_mutex when awaiting replies as it's slow
    // unnecessary
    for (auto &[worker_id, worker_stats] : worker_stats_map)
    {
        AppendIntervalStatsUpdate(pipe, std::to_string(worker_id.worker_id),
                                  EnumName<WORKER>(), update_time_ms,
                                  worker_stats);
        if (worker_stats.interval_hashrate > 0) pool_worker_count++;

        worker_stats.ResetInterval();
        pool_hr += worker_stats.interval_hashrate;
    }

    for (auto &[miner_id, miner_stats] : miner_stats_map)
    {
        std::string hr_str(std::to_string(miner_stats.interval_hashrate));
        std::string miner_id_str(std::to_string(miner_id));

        AppendIntervalStatsUpdate(pipe, miner_id_str, EnumName<MINER>(),
                                  update_time_ms, miner_stats);

        pipe.command("ZADD"sv, key_names.miner_index_hashrate, hr_str,
                     std::to_string(miner_id));

        AppendUpdateWorkerCount(pipe, miner_id, miner_stats.worker_count,
                                update_time_ms);

        if (miner_stats.interval_hashrate > 0) pool_miner_count++;

        miner_stats.ResetInterval();
    }
    stats_mutex.unlock();

    // net hr
    AppendTsAdd(pipe, key_names.hashrate_network, update_time_ms,
                ns.network_hr);

    // diff
    AppendTsAdd(pipe, key_names.difficulty, update_time_ms, ns.difficulty);

    // pool hr, workers, miners
    AppendTsAdd(pipe, key_names.hashrate_pool, update_time_ms, pool_hr);
    AppendTsAdd(pipe, key_names.worker_count_pool, update_time_ms,
                pool_worker_count);
    AppendTsAdd(pipe, key_names.miner_count, update_time_ms, pool_miner_count);

    return true;
    // return GetReplies();
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

bool RedisStats::CreateMinerStats(std::string_view addr_lowercase,
                                  std::optional<std::string_view> alias, MinerId id,
                                  int64_t curime_ms)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    auto pipe =redis->pipeline(false);
    {
        auto chain = key_names.coin;

        std::string id_str = std::to_string(id);

        std::string curime_ms_str = std::to_string(curime_ms / 1000);

        // reset all indexes of new miner
        pipe.command("ZADD"sv, key_names.miner_index_round_effort, "0", id_str);
        pipe.command("ZADD"sv, key_names.miner_index_hashrate, "0", id_str);

        // to be accessible by lowercase addr
        AppendCreateStatsTsMiner(pipe, id_str, alias, addr_lowercase,
                                 curime_ms);
    }

    return true;
    // return GetReplies();
}

bool RedisStats::CreateWorkerStats(FullId full_id,
                                   std::string_view address_lowercase,
                                   std::string_view worker_name,
                                   std::optional<std::string_view> alias,
                                   uint64_t curtime_ms)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    auto pipe =redis->pipeline(false);

    AppendCreateStatsTsWorker(pipe, std::to_string(full_id.worker_id), alias,
                              address_lowercase, worker_name, curtime_ms);

    return true;
    // return GetReplies();
}

void RedisStats::AppendUpdateWorkerCount(sw::redis::Pipeline &pipe,
                                         MinerId miner_id, int amount,
                                         int64_t update_time_ms)
{
    AppendTsAdd(pipe,
                Format({key_names.worker_count, std::to_string(miner_id)}),
                update_time_ms, amount);
}

void RedisStats::AppendCreateStatsTsMiner(sw::redis::Pipeline &pipe,
                                          std::string_view addr,
                                          std::optional<std::string_view> id,
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
        AddressTimeSeries addr_ts{retention, addr_lowercase_sv, id};
        AppendTsCreate(pipe, key, addr_ts);
    }

    // reset worker count
    // AppendTsAdd(worker_count_key, curime_ms, 0);

    // both were already created
    auto hr_key = Format({key_names.hashrate, prefix, addr});
    auto hr_avg_key = Format({key_names.hashrate_average, prefix, addr});
    pipe.command("TS.CREATERULE", hr_key, hr_avg_key, "ROLL_AGGREGATION",
                 "roll_avg", average_hashrate_ratio_str);
}

void RedisStats::AppendCreateStatsTsWorker(sw::redis::Pipeline &pipe,
                                           std::string_view addr,
                                           std::optional<std::string_view> id,
                                           std::string_view addr_lowercase_sv,
                                           std::string_view worker_name,
                                           uint64_t curtime_ms)
{
    using namespace std::literals;

    auto retention = conf->redis.hashrate_ttl_seconds * 1000;
    constexpr auto prefix = EnumName<Prefix::WORKER>();

    for (std::string_view key_type :
         {key_names.hashrate, key_names.hashrate_average,
          key_names.shares_valid, key_names.shares_stale,
          key_names.shares_invalid})
    {
        auto key = Format({key_type, prefix, addr});
        WorkerTimeSeries worker_ts{retention, addr_lowercase_sv, id,
                                   worker_name};
        AppendTsCreate(pipe, key, worker_ts);
    }

    auto hr_key = Format({key_names.hashrate, prefix, addr});
    auto hr_avg_key = Format({key_names.hashrate_average, prefix, addr});
    pipe.command("TS.CREATERULE", hr_key, hr_avg_key, "ROLL_AGGREGATION",
                 "roll_avg", average_hashrate_ratio_str);
}

bool RedisStats::PopWorker(WorkerId fullid)
{
    std::scoped_lock lock(rc_mutex);

    // set pending inactive, (set inactive after payout check...)
    // pipe.command("SREM", key_names.active_ids_map,
    // fullid.miner_id.GetHex()}); pipe.command("SADD",
    // key_names.active_ids_map,
    //                fmt::format("-{}", fullid.miner_id.GetHex())});
    return true;
    // return GetReplies();
}