#include "./stats_manager.hpp"

int StatsManager::hashrate_interval_seconds;
int StatsManager::effort_interval_seconds;
int StatsManager::average_hashrate_interval_seconds;
int StatsManager::hashrate_ttl_seconds;
int StatsManager::diff_adjust_seconds;
double StatsManager::average_interval_ratio;

StatsManager::StatsManager(RedisManager* redis_manager,
                           DifficultyManager* diff_manager,
                           RoundManager* round_manager, int hr_interval,
                           int effort_interval, int avg_hr_interval,
                           int diff_adjust_seconds, int hashrate_ttl)
    : redis_manager(redis_manager),
      diff_manager(diff_manager),
      round_manager(round_manager)
{
    StatsManager::hashrate_interval_seconds = hr_interval;
    StatsManager::effort_interval_seconds = effort_interval;
    StatsManager::average_hashrate_interval_seconds = avg_hr_interval;
    StatsManager::diff_adjust_seconds = diff_adjust_seconds;
    StatsManager::hashrate_ttl_seconds = hashrate_ttl;
    StatsManager::average_interval_ratio = (double)avg_hr_interval / hr_interval;
}
void StatsManager::Start(std::stop_token st)
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

    if (!LoadAvgHashrateSums(next_interval_update * 1000))
    {
        Logger::Log(LogType::Critical, LogField::StatsManager,
                    "Failed to load hashrate sums!");
    }

    while (!st.stop_requested())
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Next stats update in: {}", next_effort_update);

        int64_t next_update = std::min(
            {next_interval_update, next_effort_update, next_diff_update});
        int64_t update_time_ms = next_update * 1000;

        std::this_thread::sleep_until(system_clock::from_time_t(next_update));

        if (next_update == next_interval_update)
        {
            next_interval_update += hashrate_interval_seconds;
            UpdateIntervalStats(update_time_ms);
        }

        // possible that both need to be updated at the same time
        if (next_update == next_effort_update)
        {
            next_effort_update += effort_interval_seconds;
            round_manager->UpdateEffortStats(update_time_ms);
        }

        if (next_update == next_diff_update)
        {
            diff_manager->Adjust(diff_adjust_seconds, next_diff_update);
            next_diff_update += diff_adjust_seconds;
        }

        auto startChrono = system_clock::now();

        auto endChrono = system_clock::now();
        auto duration = duration_cast<microseconds>(endChrono - startChrono);
    }
}

bool StatsManager::UpdateIntervalStats(int64_t update_time_ms)
{
    using namespace std::string_view_literals;

    // Logger::Log(LogType::Info, LogField::StatsManager,
    //             "Updating interval stats for, {} workers",
    //             worker_stats_map.size());

    miner_map miner_stats_map;
    const int64_t remove_time =
        update_time_ms - average_hashrate_interval_seconds * 1000;

    std::vector<std::pair<std::string, double>> remove_worker_hashrates;
    bool res = redis_manager->TsMrange(remove_worker_hashrates, "worker"sv,
                                       PrefixKey<Prefix::HASHRATE>(), remove_time,
                                       remove_time);

    {
        // only lock after receiving the hashrates to remove
        // to avoid unnecessary locking
        std::scoped_lock lock(stats_map_mutex);

        for (auto& [worker, worker_hr] : remove_worker_hashrates)
        {
            worker_stats_map[worker].average_hashrate_sum -= worker_hr;
        }

        for (auto& [worker, ws] : worker_stats_map)
        {
            ws.interval_hashrate =
                GetExpectedHashes(ws.current_interval_effort) / (double)hashrate_interval_seconds;

            ws.average_hashrate_sum += ws.interval_hashrate;

            ws.average_hashrate = ws.average_hashrate_sum /
                                  average_interval_ratio;

            std::string addr = worker.substr(0, ADDRESS_LEN);

            auto& miner_stats = miner_stats_map[addr];
            miner_stats.average_hashrate += ws.average_hashrate;
            miner_stats.interval_hashrate += ws.interval_hashrate;

            miner_stats.interval_valid_shares += ws.interval_valid_shares;
            miner_stats.interval_invalid_shares += ws.interval_invalid_shares;
            miner_stats.interval_stale_shares += ws.interval_stale_shares;

            if (ws.interval_hashrate > 0.d) miner_stats.worker_count++;
        }
    }
    return redis_manager->UpdateIntervalStats(worker_stats_map, miner_stats_map,
                                              &stats_map_mutex, update_time_ms);
}

bool StatsManager::LoadAvgHashrateSums(int64_t hr_time)
{
    std::vector<std::pair<std::string, double>> vec;
    bool res = redis_manager->LoadAverageHashrateSum(vec, "worker", hr_time);

    if (!res)
    {
        Logger::Log(LogType::Critical, LogField::StatsManager,
                    "Failed to load average hashrate sum!");
        return false;
    }

    for (const auto& [worker, avg_hr_sum] : vec)
    {
        worker_stats_map[worker].average_hashrate_sum = avg_hr_sum;
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Loaded worker hashrate sum for {} of {}", worker,
                    avg_hr_sum);
    }
    return true;
}

void StatsManager::AddShare(const std::string& worker_full,
                            const std::string& miner_addr, const double diff)
{
    std::scoped_lock lock(stats_map_mutex);
    // both must exist, as we added in AddWorker
    WorkerStats* worker_stats = &worker_stats_map[worker_full];

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
        // const double expected_shares = GetExpectedHashes(diff);

        worker_stats->interval_valid_shares++;
        // worker_stats->current_interval_effort += expected_shares;
        worker_stats->current_interval_effort += diff;

        Logger::Log(LogType::Debug, LogField::StatsManager,
                    "Logged share with diff: {} for {} total diff: {}", diff, worker_full, worker_stats->current_interval_effort);
        // Logger::Log(LogType::Debug, LogField::StatsManager,
        //             "Logged share with diff: {}, hashes: {}", diff,
        //             expected_shares);
    }
}

bool StatsManager::AddWorker(const std::string& address,
                             const std::string& worker_full,
                             std::string_view script_pub_key, int64_t curtime,
                             const std::string& idTag)
{
    using namespace std::literals;
    std::scoped_lock stats_db_lock(stats_map_mutex);

    bool new_worker = !worker_stats_map.contains(worker_full);
    bool new_miner = !round_manager->IsMinerIn(address);
    bool returning_worker =
        !new_worker && !worker_stats_map[worker_full].connection_count;

    std::string addr_lowercase(address);
    std::transform(addr_lowercase.begin(), addr_lowercase.end(),
                   addr_lowercase.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // 100% new, as we loaded all existing miners
    if (new_miner)
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "New miner has spawned: {}", address);
        if (redis_manager->AddNewMiner(address, addr_lowercase, worker_full,
                                       idTag, script_pub_key, curtime))
        {
            Logger::Log(LogType::Info, LogField::StatsManager,
                        "Miner {} added to database.", address);
        }
        else
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to add Miner {} to database.", address);
            return false;
        }
    }

    if (new_worker || returning_worker)
    {
        if (redis_manager->AddNewWorker(address, addr_lowercase, worker_full,
                                        idTag))
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
    }

    worker_stats_map[worker_full].connection_count++;

    return true;
}

void StatsManager::PopWorker(const std::string& worker_full,
                             const std::string& address)
{
    std::scoped_lock stats_lock(stats_map_mutex);

    int con_count = (--worker_stats_map[worker_full].connection_count);
    // dont remove from the umap in case they join back, so their progress
    // is saved
    // if (con_count == 0)
    // {
    //     // redis_manager->PopWorker(address);
    // }
}

// TODO: idea: since writing all shares on all pbaas is expensive every 10
// sec, just write to primary and to side chains write every 5 mins
// TODO: check that ts indeed exists