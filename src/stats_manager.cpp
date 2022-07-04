#include "./stats_manager.hpp"

int StatsManager::hashrate_interval_seconds;
int StatsManager::effort_interval_seconds;
int StatsManager::average_hashrate_interval_seconds;
int StatsManager::hashrate_ttl_seconds;

StatsManager::StatsManager(int hr_interval, int effort_interval,
                           int avg_hr_interval, int hashrate_ttl)
{
    StatsManager::hashrate_interval_seconds = hr_interval;
    StatsManager::effort_interval_seconds = effort_interval;
    StatsManager::average_hashrate_interval_seconds = avg_hr_interval;
    StatsManager::hashrate_ttl_seconds = hashrate_ttl;

    LoadCurrentRound();
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

    int64_t next_hr_update =
        now - (now % hashrate_interval_seconds) + hashrate_interval_seconds;

    while (true)
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Next stats update in: %d", next_effort_update);

        int64_t next_update = std::min(next_hr_update, next_effort_update);

        bool should_update_hr = false;
        bool should_update_effort = false;

        if (next_update == next_hr_update)
        {
            should_update_hr = true;
            next_hr_update += hashrate_interval_seconds;
        }

        // possible that both need to be updated at the same time
        if (next_update == next_effort_update)
        {
            should_update_effort = true;
            next_effort_update += effort_interval_seconds;
        }

        std::this_thread::sleep_until(system_clock::from_time_t(next_update));
        auto startChrono = system_clock::now();

        int64_t update_time_ms = next_update * 1000;
        UpdateStats(should_update_effort, should_update_hr, update_time_ms);

        auto endChrono = system_clock::now();
        auto duration = duration_cast<microseconds>(endChrono - startChrono);
        // Logger::Log(LogType::Info, LogField::StatsManager,
        //             "Stats update (is_interval: %d) took %dus, performed "
        //             "%d redis commands.",
        //             should_update_hr, duration.count(), command_count);
    }
}

bool StatsManager::UpdateStats(bool update_effort, bool update_hr,
                               int64_t update_time_ms)
{
    redisReply* remove_avg_reply;
    double pool_hr = 0;
    int command_count = 0;

    std::scoped_lock lock(stats_map_mutex);
    Logger::Log(LogType::Info, LogField::StatsManager,
                "Updating stats for %d miners, %d workers, is_interval: %d",
                miner_stats_map.size(), worker_stats_map.size(), update_hr);

    for (auto& [worker, ws] : worker_stats_map)
    {
        if (update_hr)
        {
            ws.interval_hashrate =
                ws.current_interval_effort / (double)hashrate_interval_seconds;
            std::string_view addr = worker.substr(0, ADDRESS_LEN);

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
    for (auto& [miner_addr, miner_ws] : miner_stats_map)
    {
        // miner round entry
        if (update_hr)
        {
            miner_ws.interval_hashrate = miner_ws.current_interval_effort /
                                         (double)hashrate_interval_seconds;

            // miner average hashrate
            miner_ws.average_hashrate_sum += miner_ws.interval_hashrate;

            miner_ws.average_hashrate =
                miner_ws.average_hashrate_sum /
                ((double)average_hashrate_interval_seconds /
                 hashrate_interval_seconds);

            // pool hashrate
            pool_hr += miner_ws.current_interval_effort;
        }
    }

    return true;
}

bool StatsManager::LoadCurrentRound()
{
    double total_effort = redis_manager->hgetd(
        fmt::format("round:pow:{}", COIN_SYMBOL), "total_effort");

    int round_start =
        redis_manager->hgeti(fmt::format("round:pow:{}", COIN_SYMBOL), "start");
    round_map[COIN_SYMBOL].round_start_ms = round_start;
    if (round_start == 0)
    {
        round_map[COIN_SYMBOL].round_start_ms = GetCurrentTimeMs();
    }

    Logger::Log(LogType::Info, LogField::StatsManager,
                "Loaded pow round effort of: %f, started at: %" PRIi64,
                round_map[COIN_SYMBOL].pow,
                round_map[COIN_SYMBOL].round_start_ms);

    redis_manager->LoadSolvers(miner_stats_map, round_map);
    // freeReplyObject(miners_reply);
    return true;
}

void StatsManager::AddShare(std::string_view worker_full,
                            std::string_view miner_addr, double diff)
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
        this->round_map[COIN_SYMBOL].pow += diff;

        worker_stats->interval_valid_shares++;
        worker_stats->current_interval_effort += diff;

        // no need the interval shares, as its easy (fast) to calculate from
        // workers on frontend (for miner)
        miner_stats->round_effort[COIN_SYMBOL].pow += diff;
        // other miner stats are calculated from worker stats for efficiency
    }
}

bool StatsManager::AddWorker(std::string_view address,
                             std::string_view worker_full,
                             std::string_view idTag, int64_t curtime)
{
    using namespace std::literals;
    std::scoped_lock stats_db_lock(stats_map_mutex);

    // (will be true on restart too, as we don't reload worker stats)
    bool newWorker = !worker_stats_map.contains(worker_full);
    bool newMiner = !miner_stats_map.contains(address);
    bool returning_worker = !worker_stats_map[worker_full].connection_count;
    int command_count = 0;

    // 100% new, as we loaded all existing miners
    if (newMiner)
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "New miner has spawned: %.*s", address.size(),
                    address.data());
    }

    // worker has been disconnected and came back, add him again
    if (redis_manager->AddWorker(address, worker_full, idTag, curtime,
                                 newWorker, newMiner))
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Worker %.*s added to database.", worker_full.data(),
                    worker_full.size());
    }
    else
    {
        Logger::Log(LogType::Critical, LogField::StatsManager,
                    "Failed to add worker %.*s to database.",
                    worker_full.data(), worker_full.size());
        return false;
    }

    miner_stats_map[address].worker_count++;
    worker_stats_map[worker_full].connection_count++;

    return true;
}

void StatsManager::PopWorker(std::string_view worker_full,
                             std::string_view address)
{
    // db lock below
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

bool StatsManager::ClosePoWRound(std::string_view chain,
                                 const BlockSubmission* submission, double fee)
{
    // redis mutex already locked
    std::scoped_lock stats_lock(stats_map_mutex);

    return true;
}

// bool StatsManager::AppendPoSBalances(std::string_view chain, int64_t from_ms)
// {
//     redisReply* reply;
//     redisAppendCommand(rc, "TS.MGET FILTER type=mature-balance coin=%b",
//                        chain.data(), chain.length());

//     if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
//     {
//         Logger::Log(LogType::Critical, LogField::StatsManager,
//                     "Failed to get balances: %s\n", rc->errstr);
//         return false;
//     }

//     // assert type = arr (2)
//     int command_count = 0;
//     for (int i = 0; i < reply->elements; i++)
//     {
//         // key is 2d array of the following arrays: key name, (empty array -
//         // labels), values (array of tuples)
//         const redisReply* key = reply->element[i];
//         auto keyName = std::string(key[0].element[0]->str);
//         std::string minerAddr =
//             keyName.substr(keyName.find_last_of(':') + 1, keyName.size());

//         // skip key name + null value of labels
//         const redisReply* values = key[0].element[2];

//         int64_t timestamp = values->element[0]->integer;
//         const char* value_str = values->element[1]->str;
//         int64_t value = std::stoll(value_str);

//         // time staked * value staked
//         int64_t posPoints = (from_ms - timestamp) * value;

//         round_map[chain].pos += posPoints;

//         miner_stats_map[minerAddr].round_effort[chain].pos += posPoints;

//         // update pos effort
//         redisAppendCommand(
//             rc, "LSET round_entries:pos:%s 0 {\"effort\":% " PRIi64 "}",
//             minerAddr.c_str(),
//             miner_stats_map[minerAddr].round_effort[chain].pos);
//         command_count++;

//         std::cout << "timestamp: " << timestamp << " value: " << value << " "
//                   << value_str << std::endl;
//     }

//     redisAppendCommand(rc, "SET %b:round_effort_pos %" PRIi64, chain.data(),
//                        chain.length(), round_map[chain].pos);
//     command_count++;
//     // redisAppendCommand(rc, "HSET " COIN_SYMBOL ":round_effort_pos total
//     0");

//     // free balances reply
//     freeReplyObject(reply);

//     for (int i = 0; i < command_count; i++)
//     {
//         if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
//         {
//             Logger::Log(LogType::Critical, LogField::StatsManager,
//                         "Failed to append pos points: %s\n", rc->errstr);
//             return false;
//         }
//         freeReplyObject(reply);
//     }

//     return true;
// }

Round StatsManager::GetChainRound(std::string_view chain)
{
    std::lock_guard stats_lock(stats_map_mutex);
    return round_map[chain];
}

// TODO: idea: since writing all shares on all pbaas is expensive every 10 sec,
// just write to primary and to side chains write every 5 mins
// TODO: maybe create block submission manager;
// TODO: check that ts indeed exists