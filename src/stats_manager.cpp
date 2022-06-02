#include "./stats_manager.hpp"

StatsManager::StatsManager(redisContext* rc, std::mutex* rc_mutex)
    : worker_stats_map(), total_round_effort(), rc(rc), rc_mutex(rc_mutex)
{
}

void StatsManager::Start()
{
    using namespace std::chrono;

    Logger::Log(LogType::Info, LogField::StatsManager,
                "Started stats manager...");

    while (true)
    {
        // closest future divisible by stats_interval_seconds
        std::time_t now = std::time(nullptr);

        std::time_t from = now - (now % ROUND_INTERVAL_SECONDS);
        std::time_t to = from + ROUND_INTERVAL_SECONDS;
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Next stats update in: %d, current time: %d...", to, from);

        // for soeme reason, we need to wait 1s more
        std::this_thread::sleep_until(system_clock::from_time_t(to + 1));
        auto startChrono = system_clock::now();

        bool should_update_interval = from % STATS_INTERVAL_SECONDS == 0;
        // bool should_update_interval = true;
        int command_count = 0;
        uint64_t toMs = to * 1000;

        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Updating stats for %d workers, is_interval: %d.",
                    worker_stats_map.size(), should_update_interval);

        rc_mutex->lock();
        stats_map_mutex.lock();
        for (auto& ws_entry : worker_stats_map)
        {
            auto worker = &ws_entry.first;
            WorkerStats* ws = &ws_entry.second;
            auto minerAddr = worker->substr(0, worker->find('.'));

            if (should_update_interval)
            {
                double interval_hashrate =
                    ws->interval_effort / ROUND_INTERVAL_SECONDS;

                // worker hashrate update
                redisAppendCommand(rc, "TS.ADD SP:hashrate:%s %" PRIu64 " %f",
                                   worker->c_str(), toMs, interval_hashrate);

                command_count++;
            }
        }
        for (auto& miner_entry : miner_stats_map)
        {
            auto miner_addr = &miner_entry.first;
            WorkerStats* miner_ws = &miner_entry.second;

            // miner round entry
            redisAppendCommand(rc,
                               "LSET round_entries:%s 0 {\"effort\":%f}",
                               miner_addr->c_str(), miner_ws->round_effort[COIN_SYMBOL]);
            command_count++;

            if (should_update_interval)
            {
                double interval_hashrate =
                    miner_ws->interval_effort / ROUND_INTERVAL_SECONDS;

                // interval stats update
                // miner hashrate
                redisAppendCommand(rc, "TS.ADD SP:hashrate:%s %" PRIu64 " %f",
                                   miner_addr->c_str(), toMs,
                                   interval_hashrate);

                // sorted miner hashrate set
                redisAppendCommand(rc, "ZADD SP:hashrate_set %f %s",
                                   interval_hashrate, miner_addr->c_str());
                // pool hashrate
                redisAppendCommand(rc, "TS.ADD SP:pool_hashrate %" PRIu64 " %f",
                                   toMs, interval_hashrate);

                command_count += 3;
            }
        }

        //TODO: make for each side chain every 5 mins
        redisAppendCommand(rc, "HSET " COIN_SYMBOL ":round_effort_pow total %f",
                           total_round_effort[COIN_SYMBOL]);
        command_count++;

        stats_map_mutex.unlock();

        for (int i = 0; i < command_count; i++)
        {
            redisReply* reply;
            if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::StatsManager,
                            "Failed to update stats: %s\n", rc->errstr);
                break;
            }
            freeReplyObject(reply);
        }
        rc_mutex->unlock();

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

void StatsManager::AddShare(std::string worker_full, std::string miner_addr,
                            double diff)
{
    std::lock_guard guard(stats_map_mutex);
    WorkerStats* worker_stats = &worker_stats_map[worker_full];
    WorkerStats* miner_stats = &miner_stats_map[miner_addr];

    if (diff == STALE_SHARE_DIFF)
    {
        worker_stats->interval_stale_shares++;
    }
    else if (diff == INVALID_SHARE_DIFF)
    {
        worker_stats->interval_invalid_shares++;
    }
    else
    {
        this->total_round_effort[COIN_SYMBOL] += diff;

        worker_stats->round_effort[COIN_SYMBOL] += diff;
        worker_stats->interval_valid_shares++;
        worker_stats->interval_effort += diff;

        // no need the interval shares, as its easy (fast) to calculate from
        // workers on frontend
        miner_stats->round_effort[COIN_SYMBOL] += diff;
    }
}

bool StatsManager::ClosePoWRound(std::string chain, BlockSubmission& submission,
                                 bool accepted, double fee)
{
    std::scoped_lock stats_lock(stats_map_mutex, *rc_mutex);

    double total_effort = total_round_effort[COIN_SYMBOL];
    double block_reward = submission.job->GetBlockReward() / 1e8;
    uint32_t height = submission.height;
    int command_amount = 0;

    // reset for next round
    total_round_effort[COIN_SYMBOL] = 0;

    redisAppendCommand(rc, "HSET " COIN_SYMBOL ":round_effort_pow total 0");
    command_amount++;


    if(!accepted){
        block_reward = 0;
    }

    for (std::pair<std::string, WorkerStats> miner_entry : miner_stats_map)
    {
        std::string* miner_addr = &miner_entry.first;
        WorkerStats* miner_stats = &miner_entry.second;
        double miner_effort = miner_stats->round_effort[COIN_SYMBOL];

        double miner_share = miner_effort / total_effort;
        double miner_reward = block_reward * miner_share * (1 - fee);

        // reset for next round
        redisAppendCommand(rc, "LPUSH round_entries:%s {\"effort\":0}",
                           miner_addr->c_str());

        command_amount++;

        redisAppendCommand(rc,
                           "LSET round_entries:%s 0 {\"height\":%d,\"effort\":%f,"
                           "\"share\":%f,\"reward\":%f}",
                           miner_addr->c_str(), height, miner_stats->round_effort[COIN_SYMBOL],
                           miner_share, miner_reward);
        command_amount++;

        redisAppendCommand(rc,
                           "TS.INCRBY "
                           "%s:balance:%s %f"
                           " TIMESTAMP %" PRId64,
                           chain.c_str(), miner_addr->c_str(), miner_reward,
                           submission.timeMs);
        command_amount++;

        miner_stats->round_effort[COIN_SYMBOL] = 0;

        Logger::Log(LogType::Debug, LogField::Redis,
                    "Round: %d, miner: %s, effort: %f, share: %f, reward: "
                    "%f, total effort: %f",
                    height, miner_addr->c_str(), miner_effort, miner_share,
                    miner_reward, total_effort);
    }

    for (int i = 0; i < command_amount; i++)
    {
        redisReply* reply;
        if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to close round: %s\n", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
    }
    return true;
}

double StatsManager::GetTotalEffort(std::string chain)
{
    std::lock_guard stats_lock(stats_map_mutex);
    return total_round_effort[COIN_SYMBOL];
}

// TODO: idea: since writing all shares on all pbaas is expensive every 10 sec,
// just write to primary and to side chains write every 5 mins