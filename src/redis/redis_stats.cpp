#include "redis_manager.hpp"
#include "stats_manager.hpp"

using enum Prefix;

void RedisManager::AppendIntervalStatsUpdate(std::string_view addr,
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

bool RedisManager::UpdateIntervalStats(worker_map &worker_stats_map,
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

bool RedisManager::LoadAverageHashrateSum(
    std::vector<std::pair<WorkerFullId, double>> &hashrate_sums,
    std::string_view prefix, int64_t hr_time)
{
    int64_t from =
        hr_time - StatsManager::average_hashrate_interval_seconds * 1000;

    TsAggregation aggregation{
        .type = "SUM",
        .time_bucket_ms =
            StatsManager::average_hashrate_interval_seconds * 1000};
    return TsMrange(hashrate_sums, prefix, key_names.hashrate, from, hr_time,
                    &aggregation);
}

bool RedisManager::ResetMinersWorkerCounts(const efforts_map_t &miner_stats_map,
                                           int64_t time_now)
{
    using namespace std::string_view_literals;
    for (const auto &[id, _] : miner_stats_map)
    {
        // reset worker count
        AppendUpdateWorkerCount(id, 0, time_now);
    }

    return GetReplies();
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