#include "./stats_manager.hpp"

void StatsManager::Start(redisContext* rc)
{
    using namespace std::chrono;

    Logger::Log(LogType::Info, LogField::StatsManager, "Started stats manager...");

    while (true)
    {
        // closest future divisible by stats_interval_seconds
        std::time_t now = time(0);

        std::time_t from = now - (now % ROUND_INTERVAL_SECONDS);
        std::time_t to = from + ROUND_INTERVAL_SECONDS;

        std::this_thread::sleep_until(system_clock::from_time_t(to));

        auto startChrono = system_clock::now();

        bool should_update_interval = from % STATS_INTERVAL_SECONDS == 0;
        int command_count = 0;
        uint64_t toMs = to * 1000;

        std::unordered_map<std::string, WorkerStats> miner_stats;

        for (auto& ws_entry : worker_stats_map)
        {
            std::string* worker = &ws_entry.first;
            WorkerStats* ws = &ws_entry.second;
            std::string minerAddr = worker->substr(0, worker->find(':'));

            miner_stats[minerAddr].round_effort += ws->round_effort;

            if (should_update_interval)
            {
                miner_stats[minerAddr].interval_effort += ws->interval_effort;
                miner_stats[minerAddr].interval_valid_shares +=
                    ws->interval_valid_shares;
                miner_stats[minerAddr].interval_invalid_shares +=
                    ws->interval_invalid_shares;

                double interval_hashrate =
                    ws->interval_effort / ROUND_INTERVAL_SECONDS;

                // worker hashrate update
                redisAppendCommand(rc, "TS.ADD SP:hashrate:%s %" PRIu64 " %f",
                                   worker->c_str(), toMs, interval_hashrate);

                command_count++;
            }
        }

        for (auto& miner_entry : miner_stats)
        {
            auto miner_addr = &miner_entry.first;
            WorkerStats *miner_ws = &miner_entry.second;

            // miner round entry
            redisAppend("HSET round_entry:%s %f", miner_addr->c_str(),
                        miner_ws->round_effort);
            command_count++;

            if (should_update_interval)
            {
                double interval_hashrate =
                    miner_ws->interval_effort / ROUND_INTERVAL_SECONDS;

                // interval stats update
                // miner hashrate
                redisAppendCommand(rc, "TS.ADD SP:hashrate:%s %" PRIu64 " %f",
                                   miner_addr->c_str(), toMs, interval_hashrate);

                // sorted miner hashrate set
                redisAppendCommand(rc, "ZADD SP:hashrate_set %d %s",
                                   interval_hashrate, miner_addr->c_str());
                // pool hashrate
                redisAppendCommand(rc, "TS.ADD SP:pool_hashrate %" PRIu64 " %f",
                                   toMs, interval_hashrate);

                command_count += 3;
            }
        }
        miner_stats.clear();

        for (int i = 0; i < command_count; i++)
        {
            redisReply* reply;
            if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
            {
                printf("Error: %s\n", rc->errstr);
                break;
            }
            freeReplyObject(reply);
        }

        auto endChrono = system_clock::now();
        auto duration = duration_cast<microseconds>(endChrono - startChrono);
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Stats update (is_interval: %d) took %dus, performed "
                    "%d redis commands.",
                    should_update_interval, duration.count(), command_count);
    }
}

void StatsManager::LoadCurrentRound()
{
    // TODO
}

void StatsManager::AddShare(std::string worker, double diff)
{
    auto it = worker_stats_map.find(worker);

    if (it == worker_stats_map.end())
    {
        it = worker_stats_map.insert(std::make_pair(worker, WorkerStats()))
                 .first;
    }

    WorkerStats* stats = &(*it).second;
    if (diff == STALE_SHARE_DIFF)
    {
        stats->interval_stale_shares++;
    }
    else if (diff == INVALID_SHARE_DIFF)
    {
        stats->interval_invalid_shares++;
    }
    else
    {
        stats->interval_valid_shares++;
        stats->round_effort += diff;
        stats->interval_effort += diff;
    }
}