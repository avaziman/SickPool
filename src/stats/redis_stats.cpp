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

    // average hashrate
    key = Format({key_names.hashrate_average, prefix_addr});
    AppendTsAdd(key, update_time_ms, ws.average_hashrate);

    // shares
    key = Format({key_names.shares_valid, prefix_addr});
    AppendTsAdd(key, update_time_ms, ws.interval_valid_shares);

    key = Format({key_names.shares_invalid, prefix_addr});
    AppendTsAdd(key, update_time_ms, ws.interval_invalid_shares);

    key = Format({key_names.shares_stale, prefix_addr});
    AppendTsAdd(key, update_time_ms, ws.interval_stale_shares);
}

bool RedisStats::UpdateIntervalStats(worker_map &worker_stats_map,
                                     miner_map &miner_stats_map,
                                     std::mutex *stats_mutex, double net_hr,
                                     double diff, uint32_t blocks_found,
                                     int64_t update_time_ms)
{
    using namespace std::string_view_literals;
    std::scoped_lock lock(rc_mutex);

    double pool_hr = 0;
    uint32_t pool_worker_count = 0;
    uint32_t pool_miner_count = 0;

    {
        // don't lock stats_mutex when awaiting replies as it's slow
        // unnecessary
        std::scoped_lock stats_lock(*stats_mutex);

        for (auto &[worker_id, worker_stats] : worker_stats_map)
        {
            AppendIntervalStatsUpdate(worker_id.GetHex(), EnumName<WORKER>(),
                                      update_time_ms, worker_stats);
            if (worker_stats.interval_hashrate > 0) pool_worker_count++;

            worker_stats.ResetInterval();
            pool_hr += worker_stats.interval_hashrate;
        }

        for (auto &[miner_addr, miner_stats] : miner_stats_map)
        {
            std::string hr_str = std::to_string(miner_stats.interval_hashrate);

            AppendIntervalStatsUpdate(miner_addr.GetHex(), EnumName<MINER>(),
                                      update_time_ms, miner_stats);

            AppendCommand({"ZADD"sv, key_names.solver_index_hashrate, hr_str,
                           miner_addr.GetHex()});

            AppendHset(Format({key_names.solver, miner_addr.GetHex()}),
                       EnumName<Prefix::HASHRATE>(), hr_str);

            AppendUpdateWorkerCount(miner_addr, miner_stats.worker_count,
                                    update_time_ms);

            if (miner_stats.interval_hashrate > 0) pool_miner_count++;

            miner_stats.ResetInterval();
        }
    }
    // net hr
    AppendTsAdd(key_names.hashrate_network, update_time_ms, net_hr);

    // diff
    AppendTsAdd(key_names.difficulty, update_time_ms, diff);

    // pool hr, workers, miners
    AppendTsAdd(key_names.hashrate_pool, update_time_ms, pool_hr);
    AppendTsAdd(key_names.worker_count_pool, update_time_ms, pool_miner_count);
    AppendTsAdd(key_names.miner_count, update_time_ms, pool_worker_count);

    return GetReplies();
}

bool RedisStats::LoadAverageHashrateSum(
    std::vector<std::pair<WorkerFullId, double>> &hashrate_sums,
    std::string_view prefix, int64_t hr_time, int64_t period)
{
    int64_t from = hr_time - period;

    TsAggregation aggregation{.type = "SUM", .time_bucket_ms = period};
    return TsMrange(hashrate_sums, prefix, key_names.hashrate, from, hr_time,
                    &aggregation);
}

bool RedisStats::ResetMinersWorkerCounts(
    const std::vector<MinerIdHex> &addresses, int64_t time_now)
{
    using namespace std::string_view_literals;
    for (const auto &[id, _] : addresses)
    {
        // reset worker count
        AppendUpdateWorkerCount(id, 0, time_now);
    }

    return GetReplies();
}

bool RedisStats::AddNewMiner(std::string_view address,
                             std::string_view addr_lowercase,
                             std::string_view alias, const MinerIdHex &id,
                             int64_t curtime, int64_t min_payout)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction add_worker_tx(this);

        auto chain = key_names.coin;

        std::string_view id_hex = id.GetHex();

        // address map
        AppendCommand(
            {"HSET"sv, key_names.address_id_map, addr_lowercase, id_hex});

        if (!alias.empty())
        {
            AppendCommand({"HSET"sv, key_names.address_id_map, alias, id_hex});
        }

        // create worker count ts
        std::string worker_count_key = Format({key_names.worker_count, id_hex});

        AppendTsCreate(worker_count_key, EnumName<MINER>(),
                       EnumName<WORKER_COUNT>(), addr_lowercase, alias,
                       conf->redis.hashrate_ttl_seconds * 1000);

        // reset worker count
        AppendTsAdd(worker_count_key, curtime, 0);

        std::string curtime_str = std::to_string(curtime);

        // reset all indexes of new miner
        AppendCommand(
            {"ZADD"sv, key_names.solver_index_jointime, curtime_str, id_hex});
        AppendCommand(
            {"ZADD"sv, key_names.solver_index_worker_count, "0", id_hex});
        AppendCommand({"ZADD"sv, key_names.solver_index_mature, "0", id_hex});
        AppendCommand({"ZADD"sv, key_names.solver_index_hashrate, "0", id_hex});

        auto solver_key = Format({key_names.solver, id_hex});
        // add alias here
        AppendCommand({"HSET"sv,
                       solver_key,
                       EnumName<ADDRESS>(),
                       address,
                       EnumName<ALIAS>(),
                       alias,
                       EnumName<START_TIME>(),
                       curtime_str,
                       EnumName<PAYOUT_THRESHOLD>(),
                       std::to_string(min_payout),
                       EnumName<PAYOUT_FEELESS>(),
                       "0"sv,
                       EnumName<HASHRATE>(),
                       "0"sv,
                       EnumName<MATURE_BALANCE>(),
                       "0"sv,
                       EnumName<IMMATURE_BALANCE>(),
                       "0"sv,
                       EnumName<WORKER_COUNT>(),
                       "0"sv});

        // to be accessible by lowercase addr
        AppendCreateStatsTs(id_hex, alias, EnumName<MINER>(), addr_lowercase);
    }

    return GetReplies();
}

bool RedisStats::AddNewWorker(const WorkerFullId &full_id,
                              std::string_view address_lowercase,
                              std::string_view worker_name,
                              std::string_view alias)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    auto res = Command({"HSET",
                        Format({key_names.solver, EnumName<WORKER>(),
                                full_id.miner_id.GetHex()}),
                        worker_name, full_id.worker_id.GetHex()});

    {
        RedisTransaction add_worker_tx(this);

        AppendCreateStatsTs(full_id.GetHex(), alias, EnumName<WORKER>(),
                            address_lowercase);
    }

    return GetReplies();
}

void RedisStats::AppendUpdateWorkerCount(MinerIdHex miner_id, int amount,
                                         int64_t update_time_ms)
{
    using namespace std::string_view_literals;
    std::string amount_str = std::to_string(amount);

    AppendCommand({"ZADD"sv, key_names.solver_index_worker_count,
                   miner_id.GetHex(), amount_str});

    AppendCommand({"HSET"sv, Format({key_names.solver, miner_id.GetHex()}),
                   EnumName<WORKER_COUNT>(), amount_str});

    AppendCommand({"TS.ADD"sv,
                   Format({key_names.worker_count, miner_id.GetHex()}),
                   std::to_string(update_time_ms), amount_str});
}

void RedisStats::AppendCreateStatsTs(std::string_view addrOrWorker,
                                     std::string_view id,
                                     std::string_view prefix,
                                     std::string_view addr_lowercase_sv)
{
    using namespace std::literals;

    auto retention = conf->redis.hashrate_ttl_seconds * 1000;

    for (std::string_view key_type :
         {key_names.hashrate, key_names.hashrate_average,
          key_names.shares_valid, key_names.shares_stale,
          key_names.shares_invalid})
    {
        auto key = fmt::format("{}:{}:{}", key_type, prefix, addrOrWorker);
        AppendTsCreate(key, prefix, key_type, addr_lowercase_sv, id, retention);
    }
}

bool RedisStats::GetMinerId(MinerIdHex &id_res, std::string_view addr_lc)
{
    std::scoped_lock lock(rc_mutex);

    auto res = Command({"HGET", key_names.address_id_map, addr_lc});

    if (res->type != REDIS_REPLY_STRING)
    {
        return false;
    }

    uint32_t id;
    std::from_chars(res->str, res->str + res->len, id, 16);

    id_res = MinerIdHex(id);

    return true;
}

bool RedisStats::GetWorkerId(WorkerIdHex &worker_id, const MinerIdHex &miner_id,
                             std::string_view worker_name)
{
    std::scoped_lock lock(rc_mutex);

    auto res = Command(
        {"HGET",
         Format({key_names.solver, EnumName<WORKER>(), miner_id.GetHex()}),
         worker_name});

    if (res->type != REDIS_REPLY_STRING)
    {
        return false;
    }

    uint32_t id;
    std::from_chars(res->str, res->str + res->len, id, 16);

    worker_id = WorkerIdHex(id);

    return true;
}

bool RedisStats::PopWorker(const WorkerFullId &fullid)
{
    std::scoped_lock lock(rc_mutex);

    // set pending inactive, (set inactive after payout check...)
    AppendCommand({"SREM", key_names.active_ids_map, fullid.miner_id.GetHex()});
    AppendCommand({"SADD", key_names.active_ids_map, fmt::format("-{}", fullid.miner_id.GetHex())});

    return GetReplies();
}