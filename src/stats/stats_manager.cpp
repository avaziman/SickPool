#include "./stats_manager.hpp"

template void StatsManager::Start<ZanoStatic>(std::stop_token st);

uint32_t StatsManager::average_interval_ratio;

StatsManager::StatsManager(const PersistenceLayer& pl,
                           DifficultyManager* diff_manager,
                           RoundManager* round_manager, const StatsConfig* cc)
    : conf(cc),
      persistence_stats(pl),
      diff_manager(diff_manager),
      round_manager(round_manager)
{
    StatsManager::average_interval_ratio =
        cc->average_hashrate_interval_seconds / cc->hashrate_interval_seconds;
}

template <StaticConf confs>
void StatsManager::Start(std::stop_token st)
{
    using namespace std::chrono;

    logger.Log<LogType::Info>("Started stats manager...");
    int64_t now = std::time(nullptr);

    // rounded to the nearest divisible effort_internval_seconds
    int64_t next_effort_update = now - (now % conf->effort_interval_seconds) +
                                 conf->effort_interval_seconds;

    int64_t next_interval_update = now -
                                   (now % conf->hashrate_interval_seconds) +
                                   conf->hashrate_interval_seconds;

    int64_t next_diff_update =
        now - (now % conf->diff_adjust_seconds) + conf->diff_adjust_seconds;

    while (!st.stop_requested())
    {
        int64_t next_update = std::min(
            {next_interval_update, next_effort_update, next_diff_update});

        // logger.Log<LogType::Info>(
        //             "Next stats update in: {}", next_update);

        int64_t update_time_ms = next_update * 1000;

        std::this_thread::sleep_until(system_clock::from_time_t(next_update));

        if (next_update == next_interval_update)
        {
            next_interval_update += conf->hashrate_interval_seconds;
            UpdateIntervalStats<confs>(update_time_ms);
            // no need to lock as its after the stats update and adding shares
            // doesnt affect it
            std::unique_lock _(to_remove_mutex);
            for (auto it = to_remove.begin(); it != to_remove.end();)
            {
                if (it->second >= average_interval_ratio)
                {
                    worker_stats_map.erase(it->first);
                    it = to_remove.erase(it);
                }
                else
                {
                    it->second++;
                    ++it;
                }
            }
        }

        // possible that both need to be updated at the same time
        if (next_update == next_effort_update)
        {
            next_effort_update += conf->effort_interval_seconds;
            round_manager->UpdateEffortStats(update_time_ms);

#if PAYMENT_SCHEME == PAYMENT_SCHEME_PPLNS
            round_manager->PushPendingShares();
#endif
        }

        if (next_update == next_diff_update)
        {
            diff_manager->Adjust(conf->diff_adjust_seconds, next_diff_update);
            next_diff_update += conf->diff_adjust_seconds;
        }

        auto startChrono = system_clock::now();

        auto endChrono = system_clock::now();
        auto duration = duration_cast<microseconds>(endChrono - startChrono);
    }

    logger.Log<LogType::Info>("Stopped stats manager on thread {}", gettid());
}

template <StaticConf confs>
bool StatsManager::UpdateIntervalStats(int64_t update_time_ms)
{
    using namespace std::string_view_literals;

    // logger.Log<LogType::Info>(
    //             "Updating interval stats for, {} workers",
    //             worker_stats_map.size());

    miner_map miner_stats_map;

    std::unique_lock stats_unique_lock(stats_list_smutex);

    for (auto& [worker_id, ws] : worker_stats_map)
    {
        ws.interval_hashrate =
            GetExpectedHashes<confs>(ws.current_interval_effort) /
            (double)conf->hashrate_interval_seconds;

        auto& miner_stats = miner_stats_map[worker_id.miner_id];
        miner_stats.interval_hashrate += ws.interval_hashrate;

        miner_stats.interval_valid_shares += ws.interval_valid_shares;
        miner_stats.interval_invalid_shares += ws.interval_invalid_shares;
        miner_stats.interval_stale_shares += ws.interval_stale_shares;

        if (ws.interval_hashrate > 0.d) miner_stats.worker_count++;
    }

    return persistence_stats.UpdateIntervalStats(
        worker_stats_map, miner_stats_map, std::move(stats_unique_lock),
        network_stats, update_time_ms);
}

void StatsManager::AddShare(const worker_map::iterator& it, const double diff)
{
    // can be added simultaneously as each client has its own stats object
    std::shared_lock shared_lock(stats_list_smutex);
    auto& [id, worker_stats] = *it;

    if (diff == static_cast<double>(BadDiff::STALE_SHARE_DIFF))
    {
        worker_stats.interval_stale_shares++;
    }
    else if (diff == static_cast<double>(BadDiff::INVALID_SHARE_DIFF))
    {
        worker_stats.interval_invalid_shares++;
    }
    else
    {
        worker_stats.interval_valid_shares++;
        worker_stats.current_interval_effort += diff;

        logger.Log<LogType::Debug>(
            "[THREAD {}] Logged share with diff: {} for {} total diff: {}",
            gettid(), diff, id.GetHex(), worker_stats.current_interval_effort);
    }
}

bool StatsManager::AddWorker(WorkerFullId& worker_full_id,
                             worker_map::iterator& it, std::string_view address,
                             std::string_view worker_name, int64_t curtime,
                             std::string_view alias, int64_t min_payout)
{
    using namespace std::literals;

    std::string addr_lowercase;
    addr_lowercase.reserve(address.size());
    std::ranges::transform(address, std::back_inserter(addr_lowercase),
                           [](char c) { return std::tolower(c); });

    int miner_id = persistence_stats.GetMinerId(address, alias);
    if (miner_id == -1)
    {
        logger.Log<LogType::Info>("New miner has joined the pool: {}", address);

        persistence_stats.AddMiner(address, alias, curtime, min_payout);

        logger.Log<LogType::Info>("Miner {} added to database.", address);
        return AddWorker(worker_full_id, it, address, worker_name, curtime,
                         alias, min_payout);
    }
    else
    {
        // if he exists in sql db still try to add in redis just in case
        persistence_stats.AddNewMiner(address, addr_lowercase, alias, miner_id,
                                      curtime, min_payout);
    }

    // set active
    persistence_stats.SetActiveId(miner_id);

    int worker_id = persistence_stats.GetWorkerId(miner_id, worker_name);
    if (worker_id == -1)
    {
        // new id
        logger.Log<LogType::Info>("New worker has joined the pool: ",
                                  worker_name);

        persistence_stats.AddWorker(miner_id, worker_name, curtime);

        logger.Log<LogType::Info>("Worker {} added to database.", worker_name);

        return AddWorker(worker_full_id, it, address, worker_name, curtime,
                         alias, min_payout);
    }
    else
    {
        // if he exists in sql db still try to add in redis just in case
        persistence_stats.AddNewWorker(WorkerFullId(miner_id, worker_id),
                                       addr_lowercase, worker_name, alias,
                                       curtime);
    }

    worker_full_id = WorkerFullId(miner_id, worker_id);

    std::unique_lock stats_lock(stats_list_smutex);

    // constant time complexity insert
    it = worker_stats_map.emplace(worker_stats_map.cend(), worker_full_id,
                                  WorkerStats{});

    return true;
}

void StatsManager::PopWorker(const worker_map::iterator& it)
{
    std::scoped_lock _(to_remove_mutex);
    to_remove.emplace_back(it, 0);
}
// TODO: idea: since writing all shares on all pbaas is expensive every 10
// sec, just write to primary and to side chains write every 5 mins