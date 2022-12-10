#include "./stats_manager.hpp"

template void StatsManager::Start<ZanoStatic>(std::stop_token st);

int StatsManager::hashrate_interval_seconds;
int StatsManager::effort_interval_seconds;
int StatsManager::average_hashrate_interval_seconds;
int StatsManager::diff_adjust_seconds;

double StatsManager::average_interval_ratio;

StatsManager::StatsManager(const RedisManager& redis_manager,
                           DifficultyManager* diff_manager,
                           RoundManager* round_manager, const StatsConfig* cc)
    : conf(cc),
      redis_manager(redis_manager),
      diff_manager(diff_manager),
      round_manager(round_manager)
{
    StatsManager::hashrate_interval_seconds = cc->hashrate_interval_seconds;
    StatsManager::effort_interval_seconds = cc->effort_interval_seconds;
    StatsManager::average_hashrate_interval_seconds =
        cc->average_hashrate_interval_seconds;
    StatsManager::diff_adjust_seconds = cc->diff_adjust_seconds;
    StatsManager::average_interval_ratio =
        (double)average_hashrate_interval_seconds / hashrate_interval_seconds;
}

template <StaticConf confs>
void StatsManager::Start(std::stop_token st)
{
    using namespace std::chrono;

    logger.Log<LogType::Info>("Started stats manager...");
    int64_t now = std::time(nullptr);

    // rounded to the nearest divisible effort_internval_seconds
    int64_t next_effort_update =
        now - (now % effort_interval_seconds) + effort_interval_seconds;

    int64_t next_interval_update =
        now - (now % hashrate_interval_seconds) + hashrate_interval_seconds;

    int64_t next_diff_update =
        now - (now % diff_adjust_seconds) + diff_adjust_seconds;

    int64_t next_block_update =
        now - (now % conf->mined_blocks_interval) + conf->mined_blocks_interval;

    while (!st.stop_requested())
    {
        int64_t next_update =
            std::min({next_interval_update, next_effort_update,
                      next_diff_update, next_block_update});

        // logger.Log<LogType::Info>(
        //             "Next stats update in: {}", next_update);

        int64_t update_time_ms = next_update * 1000;

        std::this_thread::sleep_until(system_clock::from_time_t(next_update));

        if (next_update == next_interval_update)
        {
            next_interval_update += hashrate_interval_seconds;
            UpdateIntervalStats<confs>(update_time_ms);
            // no need to lock as its after the stats update and adding shares
            // doesnt affect it
            // TODO think how to stop updating stats while reserving right
            // statistics on disconnect
            // std::unique_lock _(to_remove_mutex);
            // for (const auto& rm_it : to_remove)
            // {
            //     worker_stats_map.erase(rm_it);
            // }
            // to_remove.clear();
        }

        // possible that both need to be updated at the same time
        if (next_update == next_effort_update)
        {
            next_effort_update += effort_interval_seconds;
            round_manager->UpdateEffortStats(update_time_ms);

#if PAYMENT_SCHEME == PAYMENT_SCHEME_PPLNS
            round_manager->PushPendingShares();
#endif
        }

        if (next_update == next_diff_update)
        {
            diff_manager->Adjust(diff_adjust_seconds, next_diff_update);
            next_diff_update += diff_adjust_seconds;
        }

        if (next_update == next_block_update)
        {
            uint32_t block_number = round_manager->blocks_found.load();
            // redis_manager.UpdateBlockNumber(update_time_ms, block_number);
            round_manager->blocks_found.store(0);

            next_block_update += conf->mined_blocks_interval;
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
            (double)hashrate_interval_seconds;

        // ws.average_hashrate_sum += ws.interval_hashrate;

        // ws.average_hashrate = ws.average_hashrate_sum /
        // average_interval_ratio;

        auto& miner_stats = miner_stats_map[worker_id.miner_id];
        // miner_stats.average_hashrate += ws.average_hashrate;
        miner_stats.interval_hashrate += ws.interval_hashrate;

        miner_stats.interval_valid_shares += ws.interval_valid_shares;
        miner_stats.interval_invalid_shares += ws.interval_invalid_shares;
        miner_stats.interval_stale_shares += ws.interval_stale_shares;

        if (ws.interval_hashrate > 0.d) miner_stats.worker_count++;
    }

    return redis_manager.UpdateIntervalStats(
        worker_stats_map, miner_stats_map, std::move(stats_unique_lock),
        round_manager->netwrok_hr, round_manager->difficulty,
        round_manager->blocks_found, update_time_ms);
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
        // const double expected_shares = GetExpectedHashes(diff);

        worker_stats.interval_valid_shares++;
        // worker_stats->current_interval_effort += expected_shares;
        worker_stats.current_interval_effort += diff;

        logger.Log<LogType::Debug>(
            "[THREAD {}] Logged share with diff: {} for {} total diff: {}", gettid(), diff,
            id.GetHex(), worker_stats.current_interval_effort);
        // logger.Log<LogType::Debug>(
        //             "Logged share with diff: {}, hashes: {}", diff,
        //             expected_shares);
    }
}

bool StatsManager::AddWorker(WorkerFullId& worker_full_id,
                             worker_map::iterator& it, std::string_view address,
                             std::string_view worker_name, int64_t curtime,
                             std::string_view alias, int64_t min_payout)
{
    using namespace std::literals;
    std::scoped_lock stats_db_lock(stats_list_smutex);

    std::string addr_lowercase;
    addr_lowercase.reserve(address.size());
    std::ranges::transform(address, std::back_inserter(addr_lowercase),
                           [](char c) { return std::tolower(c); });

    MinerIdHex miner_id(0);
    if (bool new_miner = redis_manager.GetMinerId(miner_id, addr_lowercase);
        !new_miner)
    {
        // new id
        miner_id = MinerIdHex(redis_manager.GetMinerCount());

        logger.Log<LogType::Info>(
            "New miner has spawned: {}, assigning new id {}", address,
        miner_id.GetHex());
        if (redis_manager.AddNewMiner(address, addr_lowercase, "", miner_id,
                                      curtime, min_payout))
        {
            logger.Log<LogType::Info>("Miner {} added to database.", address);
        }
        else
        {
            logger.Log<LogType::Critical>("Failed to add Miner {} to database.",
                                          address);
            return false;
        }
    }

    // set active
    redis_manager.SetActiveId(miner_id);

    WorkerIdHex worker_id(0);
    if (bool new_worker =
            !redis_manager.GetWorkerId(worker_id, miner_id, worker_name);
        new_worker)
    {
        // new id
        worker_id = WorkerIdHex(redis_manager.GetWorkerCount(miner_id));

        if (redis_manager.AddNewWorker(WorkerFullId(miner_id.id, worker_id.id),
                                       addr_lowercase, worker_name, alias,
                                       curtime))
        {
            logger.Log<LogType::Info>("Worker {} added to database.",
                                      worker_name);
        }
        else
        {
            logger.Log<LogType::Critical>(
                "Failed to add worker {} to database.", worker_name);
            return false;
        }
    }

    worker_full_id = WorkerFullId(miner_id.id, worker_id.id);

    // constant time complexity insert
    it = worker_stats_map.emplace(worker_stats_map.cend(), worker_full_id,
                                  WorkerStats{});

    return true;
}

void StatsManager::PopWorker(worker_map::iterator& it)
{
    std::scoped_lock _(to_remove_mutex);
    to_remove.push_back(it);
}
// TODO: idea: since writing all shares on all pbaas is expensive every 10
// sec, just write to primary and to side chains write every 5 mins