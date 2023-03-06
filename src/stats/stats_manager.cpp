#include "./stats_manager.hpp"

uint32_t StatsManager::average_interval_ratio;

StatsManager::StatsManager(const PersistenceLayer& pl,
                           RoundManager* round_manager, const StatsConfig* cc,
                           double hash_multiplier)
    : conf(cc),
      persistence_stats(pl),
      round_manager(round_manager),
      hash_multiplier(hash_multiplier)
{
    StatsManager::average_interval_ratio =
        cc->average_hashrate_interval_seconds / cc->hashrate_interval_seconds;
}

void StatsManager::Start(std::stop_token st)
{
    using namespace std::chrono;

    logger.Log<LogType::Info>("Started stats manager...");
    int64_t now = GetCurrentTimeMs() / 1000;

    // rounded to the nearest divisible effort_internval_seconds
    int64_t next_effort_update = now - (now % conf->effort_interval_seconds) +
                                 conf->effort_interval_seconds;

    int64_t next_interval_update = now -
                                   (now % conf->hashrate_interval_seconds) +
                                   conf->hashrate_interval_seconds;

    while (!st.stop_requested())
    {
        int64_t next_update =
            std::min({next_interval_update, next_effort_update});

        // logger.Log<LogType::Info>(
        //             "Next stats update in: {}", next_update);

        int64_t update_time_ms = next_update * 1000;

        std::this_thread::sleep_until(system_clock::from_time_t(next_update));

        if (next_update == next_interval_update)
        {
            next_interval_update += conf->hashrate_interval_seconds;
            UpdateIntervalStats(update_time_ms);
            // no need to lock as its after the stats update and adding shares
            // doesnt affect it
            // std::unique_lock _(to_remove_mutex);
            // for (auto it = to_remove.begin(); it != to_remove.end();)
            // {
            //     if (it->second >= average_interval_ratio)
            //     {
            //         worker_stats_map.erase(it->first);
            //         it = to_remove.erase(it);
            //     }
            //     else
            //     {
            //         it->second++;
            //         ++it;
            //     }
            // }
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
    }

    logger.Log<LogType::Info>("Stopped stats manager on thread {}", gettid());
}

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
        ws.interval_hashrate = hash_multiplier * ws.current_interval_effort /
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

void StatsManager::AddValidShare(const worker_map::iterator& it,
                                 const double diff)
{
    // can be added simultaneously as each client has its own stats object
    std::shared_lock shared_lock(stats_list_smutex);
    auto& [fid, worker_stats] = *it;

    worker_stats.interval_valid_shares++;
    worker_stats.current_interval_effort += diff;

    // logger.Log<LogType::Debug>(
    //     "[THREAD {}] Logged share with diff: {} for {} total PPLNS round "
    //     "diff: {}",
    //     gettid(), diff, fid.worker_id,
    //     worker_stats.current_interval_effort);
}

void StatsManager::AddStaleShare(const worker_map::iterator& it)
{
    std::shared_lock shared_lock(stats_list_smutex);
    auto& [fid, worker_stats] = *it;

    worker_stats.interval_stale_shares++;
}

void StatsManager::AddInvalidShare(const worker_map::iterator& it)
{
    std::shared_lock shared_lock(stats_list_smutex);
    auto& [fid, worker_stats] = *it;
    worker_stats.interval_invalid_shares++;
}

bool StatsManager::AddWorker(int64_t& worker_id, int64_t miner_id,
                             std::string_view address,
                             std::string_view worker_name,
                             std::string_view alias)
{
    using namespace std::literals;

    const int64_t curtime = GetCurrentTimeMs();
    std::string addr_lowercase = ToLowerCase(address);

    // only add to sql db if doesn't exist
    if (worker_id == -1)
    {
        worker_id = PersistenceLayer::AddWorker(static_cast<MinerId>(miner_id),
                                                worker_name, curtime);

        if (worker_id == -1) return false;
    }

    // always add to redis db to make sure all timeserieses exist always
    if (!persistence_stats.CreateWorkerStats(FullId(miner_id, worker_id),
                                             addr_lowercase, worker_name, alias,
                                             curtime))
    {
        return false;
    }
    logger.Log<LogType::Info>("Worker {} added to database.", worker_name);

    // if worker disconnect and wasnt removed yet remove it to avoid stats
    // problems

    std::unique_lock stats_lock(stats_list_smutex);
    // std::erase_if(to_remove,
    //               [&](const std::pair<worker_map::iterator, uint32_t>& i)
    //               {
    //                   if (i.first->first.worker_id == full_id.worker_id)
    //                   {
    //                       worker_stats_map.erase(i.first);
    //                       return true;
    //                   }
    //                   return false;
    //               });

    // constant time complexity insert

    return true;
}

worker_map::iterator StatsManager::AddExistingWorker(FullId worker_id)
{
    return worker_stats_map.emplace(worker_stats_map.cend(), worker_id,
                                    WorkerStats{});
}

bool StatsManager::AddMiner(int64_t& miner_id, std::string_view address,
                            std::string_view alias, int64_t min_payout)
{
    const int64_t curtime = GetCurrentTimeMs();
    std::string addr_lowercase = ToLowerCase(address);

    // only add to sql db if doesn't exist
    if (miner_id == -1)
    {
        miner_id =
            PersistenceLayer::AddMiner(address, alias, curtime, min_payout);

        if (miner_id == -1) return false;
    }

    // always add to redis db to make sure all timeserieses exist always
    if (!persistence_stats.CreateMinerStats(addr_lowercase, alias, miner_id,
                                            curtime))
    {
        return false;
    }

    logger.Log<LogType::Info>("Miner {} added to database.", address);
    return true;
}

void StatsManager::PopWorker(const worker_map::iterator& it)
{
    std::scoped_lock _(to_remove_mutex);
    to_remove.emplace_back(it, 0);
}
// TODO: idea: since writing all shares on all pbaas is expensive every 10
// sec, just write to primary and to side chains write every 5 mins