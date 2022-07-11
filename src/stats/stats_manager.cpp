#include "./stats_manager.hpp"

int StatsManager::hashrate_interval_seconds;
int StatsManager::effort_interval_seconds;
int StatsManager::average_hashrate_interval_seconds;
int StatsManager::hashrate_ttl_seconds;
int StatsManager::diff_adjust_seconds;

StatsManager::StatsManager(RedisManager* redis_manager,
                           DifficultyManager* diff_manager, int hr_interval,
                           int effort_interval, int avg_hr_interval,
                           int diff_adjust_seconds, int hashrate_ttl)
    : redis_manager(redis_manager), diff_manager(diff_manager)
{
    StatsManager::hashrate_interval_seconds = hr_interval;
    StatsManager::effort_interval_seconds = effort_interval;
    StatsManager::average_hashrate_interval_seconds = avg_hr_interval;
    StatsManager::diff_adjust_seconds = diff_adjust_seconds;
    StatsManager::hashrate_ttl_seconds = hashrate_ttl;

    if (!LoadEffortHashrate())
    {
        Logger::Log(LogType::Critical, LogField::StatsManager,
                    "Failed to load current round!");
    }
}

void StatsManager::Start()
{
    using namespace std::chrono;

    Logger::Log(LogType::Info, LogField::StatsManager,
                "Started stats manager...");
    int64_t now = std::time(nullptr);

    // rounded to the nearest divisible effort_internval_seconds
    int64_t next_effort_update =
        now - (now % effort_interval_seconds) + effort_interval_seconds;

    int64_t next_interval_update =
        now - (now % hashrate_interval_seconds) + hashrate_interval_seconds;

    int64_t next_diff_update =
        now - (now % diff_adjust_seconds) + diff_adjust_seconds;

    while (true)
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Next stats update in: {}", next_effort_update);

        int64_t next_update = std::min(
            {next_interval_update, next_effort_update, next_diff_update});

        uint8_t update_flags;

        if (next_update == next_interval_update)
        {
            next_interval_update += hashrate_interval_seconds;
            update_flags |= UPDATE_INTERVAL;
        }

        // possible that both need to be updated at the same time
        if (next_update == next_effort_update)
        {
            next_effort_update += effort_interval_seconds;
            update_flags |= UPDATE_EFFORT;
        }

        if (next_update == next_diff_update)
        {
            next_diff_update += diff_adjust_seconds;
            update_flags |= UPDATE_DIFFICULTY;

            diff_manager->Adjust(diff_adjust_seconds);
        }

        std::this_thread::sleep_until(system_clock::from_time_t(next_update));
        auto startChrono = system_clock::now();

        int64_t update_time_ms = next_update * 1000;
        UpdateStats(update_time_ms, update_flags);

        auto endChrono = system_clock::now();
        auto duration = duration_cast<microseconds>(endChrono - startChrono);
    }
}

bool StatsManager::UpdateStats(int64_t update_time_ms, uint8_t update_flags)
{
    std::scoped_lock lock(stats_map_mutex);
    Logger::Log(LogType::Info, LogField::StatsManager,
                "Updating stats for {} miners, {} workers, is_interval: {}",
                miner_stats_map.size(), worker_stats_map.size(),
                update_flags & UPDATE_INTERVAL);

    if (update_flags & UPDATE_INTERVAL)
    {
        for (auto& [worker, ws] : worker_stats_map)
        {
            ws.interval_hashrate =
                ws.current_interval_effort / (double)hashrate_interval_seconds;

            if (worker.size() < ADDRESS_LEN)
            {
                Logger::Log(LogType::Error, LogField::StatsManager,
                            "Removing invalid worker name: {}", worker);
                worker_stats_map.erase(worker);
                continue;
            }
            std::string addr = worker.substr(0, ADDRESS_LEN);

            ws.average_hashrate_sum += ws.interval_hashrate;

            ws.average_hashrate = ws.average_hashrate_sum /
                                  ((double)average_hashrate_interval_seconds /
                                   hashrate_interval_seconds);

            auto& miner_stats = miner_stats_map[addr];
            miner_stats.current_interval_effort += ws.current_interval_effort;
            miner_stats.interval_valid_shares += ws.interval_valid_shares;
            miner_stats.interval_invalid_shares += ws.interval_invalid_shares;
            miner_stats.interval_stale_shares += ws.interval_stale_shares;
        }
    }
    if (update_flags & UPDATE_INTERVAL)
    {
        for (auto& [miner_addr, miner_ws] : miner_stats_map)
        {
            // miner round entry
            miner_ws.interval_hashrate = miner_ws.current_interval_effort /
                                         (double)hashrate_interval_seconds;

            // miner average hashrate
            miner_ws.average_hashrate_sum += miner_ws.interval_hashrate;

            miner_ws.average_hashrate =
                miner_ws.average_hashrate_sum /
                ((double)average_hashrate_interval_seconds /
                 hashrate_interval_seconds);
        }
    }

    return redis_manager->UpdateStats(worker_stats_map, miner_stats_map,
                                      update_time_ms, update_flags);
}

bool StatsManager::LoadEffortHashrate()
{
    return redis_manager->LoadMinersAverageHashrate(miner_stats_map);
}

void StatsManager::ResetRoundEfforts(const std::string& chain)
{
    std::scoped_lock lock(stats_map_mutex);

    for (auto& miner : miner_stats_map)
    {
        miner.second.ResetEffort(chain);
    }
}

void StatsManager::AddShare(const std::string& worker_full,
                            const std::string& miner_addr, const double diff)
{
    std::scoped_lock lock(stats_map_mutex);
    // both must exist, as we added in AddWorker
    WorkerStats* worker_stats = &worker_stats_map[worker_full];
    MinerStats* miner_stats = &miner_stats_map[miner_addr];

    if (diff == static_cast<double>(BadDiff::STALE_SHARE_DIFF))
    {
        worker_stats->interval_stale_shares++;
    }
    else if (diff == static_cast<double>(BadDiff::INVALID_SHARE_DIFF))
    {
        worker_stats->interval_invalid_shares++;
    }
    else
    {
        worker_stats->interval_valid_shares++;
        worker_stats->current_interval_effort += diff;

        miner_stats->round_effort_map[COIN_SYMBOL] += diff;
    }
}

bool StatsManager::AddWorker(const std::string& address,
                             const std::string& worker_full,
                             const std::string& idTag, int64_t curtime)
{
    using namespace std::literals;
    std::scoped_lock stats_db_lock(stats_map_mutex);

    // (will be true on restart too, as we don't reload worker stats)
    bool newWorker = !worker_stats_map.contains(worker_full);
    bool newMiner = !miner_stats_map.contains(address);
    bool returning_worker = !worker_stats_map[worker_full].connection_count;

    // 100% new, as we loaded all existing miners
    if (newMiner)
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "New miner has spawned: {}", address);
    }

    // worker has been disconnected and came back, add him again
    if (redis_manager->AddWorker(address, worker_full, idTag, curtime,
                                 newWorker, newMiner))
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Worker {} added to database.", worker_full);
    }
    else
    {
        Logger::Log(LogType::Critical, LogField::StatsManager,
                    "Failed to add worker {} to database.", worker_full);
        return false;
    }

    miner_stats_map[address].worker_count++;
    worker_stats_map[worker_full].connection_count++;

    return true;
}

void StatsManager::PopWorker(const std::string& worker_full,
                             const std::string& address)
{
    std::scoped_lock stats_lock(stats_map_mutex);

    int con_count = (--worker_stats_map[worker_full].connection_count);
    // dont remove from the umap in case they join back, so their progress is
    // saved
    if (con_count == 0)
    {
        redis_manager->PopWorker(address);
        miner_stats_map[address].worker_count--;
    }
}

void StatsManager::GetMiningEffortsReset(
    std::vector<std::pair<std::string, double>>& efforts,
    const std::string& chain_str)
{
    {
        std::scoped_lock stats_lock(stats_map_mutex);

        std::transform(miner_stats_map.begin(), miner_stats_map.end(),
                       std::back_inserter(efforts),
                       [&](auto& obj) {
                           return std::make_pair(
                               obj.first,
                               obj.second.round_effort_map[chain_str]);
                       });
    }
    ResetRoundEfforts(chain_str);
}

// TODO: idea: since writing all shares on all pbaas is expensive every 10 sec,
// just write to primary and to side chains write every 5 mins
// TODO: check that ts indeed exists