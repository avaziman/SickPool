#include "./stats_manager.hpp"

StatsManager::StatsManager(redisContext* rc, std::mutex* rc_mutex,
                           int hr_interval, int effort_interval)
    : rc(rc),
      rc_mutex(rc_mutex),
      hashrate_interval_seconds(hr_interval),
      effort_interval_seconds(effort_interval)
{
}

void StatsManager::Start()
{
    using namespace std::chrono;

    Logger::Log(LogType::Info, LogField::StatsManager,
                "Started stats manager...");

    std::time_t now = std::time(nullptr);

    // rounded to the nearest divisible effort_internval_seconds
    std::time_t next_effort_update =
        now - (now % effort_interval_seconds) + effort_interval_seconds;

    std::time_t next_hr_update =
        now - (now % hashrate_interval_seconds) + next_hr_update;

    while (true)
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Next stats update in: %d", next_effort_update);

        std::time_t next_update = std::min(next_hr_update, next_effort_update);

        bool should_update_hr = false, should_update_effort = false;
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

        int command_count = 0;
        uint64_t toMs = next_update * 1000;

        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Updating stats for %d workers, is_interval: %d.",
                    worker_stats_map.size(), should_update_hr);

        rc_mutex->lock();
        stats_map_mutex.lock();
        for (const auto& [worker, ws] : worker_stats_map)
        {
            // std::string_view minerAddr = worker.substr(0, worker.find('.'));

            if (should_update_hr)
            {
                double interval_hashrate =
                    ws.interval_effort / (double)effort_interval_seconds;

                // worker hashrate update
                redisAppendCommand(rc, "TS.ADD hashrate:%b %" PRIu64 " %f",
                                   worker.data(), worker.length(), toMs,
                                   interval_hashrate);

                command_count++;
            }
        }
        for (auto& [miner_addr, miner_ws] : miner_stats_map)
        {
            // miner round entry
            if (should_update_effort)
            {
                redisAppendCommand(
                    rc, "LSET round_entries_pow:%b 0 {\"effort\":%f}",
                    miner_addr.data(), miner_addr.length(),
                    miner_ws.round_effort[COIN_SYMBOL].pow);
                command_count++;
            }

            if (should_update_hr)
            {
                double interval_hashrate =
                    miner_ws.interval_effort / hashrate_interval_seconds;

                // interval stats update
                // miner hashrate
                redisAppendCommand(rc, "TS.ADD hashrate:%b %" PRIu64 " %f",
                                   miner_addr.data(), miner_addr.length(), toMs,
                                   interval_hashrate);

                // sorted miner hashrate set
                redisAppendCommand(rc, "ZADD hashrate_set %f %b",
                                   interval_hashrate, miner_addr.data(),
                                   miner_addr.length());
                // pool hashrate
                redisAppendCommand(rc, "TS.ADD pool_hashrate %" PRIu64 " %f",
                                   toMs, interval_hashrate);

                command_count += 3;
            }
        }

        if (should_update_effort)
        {
            // TODO: make for each side chain every 5 mins
            redisAppendCommand(rc,
                               "HSET " COIN_SYMBOL ":round_effort_pow total %f",
                               total_round_effort[COIN_SYMBOL].pow);
            command_count++;
        }
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
                    should_update_hr, duration.count(), command_count);
    }
}

void StatsManager::LoadCurrentRound()
{
    // TODO
}

// load the stats for the current round, and create entries if they don't exist
std::unordered_map<std::string_view, MinerStats>::iterator
StatsManager::LoadMinerStats(std::string_view miner_addr)
{
    std::scoped_lock lock(stats_map_mutex, *rc_mutex);
    auto it = miner_stats_map.insert({miner_addr, MinerStats()}).first;

    // if no pow or pos round entry list exists, create it
    redisReply* reply;
    int command_count = 0;

    redisAppendCommand(rc, "EXISTS round_entries_pow:%b", miner_addr.data(),
                       miner_addr.length());
    redisAppendCommand(rc, "EXISTS round_entries_pos:%b", miner_addr.data(),
                       miner_addr.length());
    // TODO: if it does exist load the effort
    if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
    {
        Logger::Log(LogType::Critical, LogField::StatsManager,
                    "Failed to check if round_entries_pow:%*.s exists: %s\n",
                    miner_addr.length(), miner_addr.data(), rc->errstr);
        return miner_stats_map.end();
    }

    if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 0)
    {
        redisAppendCommand(rc, "RPUSH round_entries_pow:%b {\"effort:\":0}",
                           miner_addr.data(), miner_addr.length());

        command_count++;
    }
    freeReplyObject(reply);

    if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
    {
        Logger::Log(LogType::Critical, LogField::StatsManager,
                    "Failed to check if round_entries_pow:%b exists: %s\n",
                    miner_addr.data(), miner_addr.length(), rc->errstr);
        return miner_stats_map.end();
    }

    if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 0)
    {
        redisAppendCommand(rc, "RPUSH round_entries_pos:%b {\"effort:\":0}",
                           miner_addr.data(), miner_addr.length());
        command_count++;
    }
    freeReplyObject(reply);

    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to create list for: %s\n", miner_addr.data(),
                        miner_addr.length(), rc->errstr);
            return miner_stats_map.end();
        }
        freeReplyObject(reply);
    }
    return it;
}

void StatsManager::AddShare(std::string_view worker_full,
                            std::string_view miner_addr, double diff)
{
    stats_map_mutex.lock();
    WorkerStats* worker_stats = &worker_stats_map[worker_full];
    auto miner_stats_it = miner_stats_map.find(miner_addr);

    if (miner_stats_it == miner_stats_map.end())
    {
        stats_map_mutex.unlock();
        LoadMinerStats(miner_addr);
        stats_map_mutex.lock();

        miner_stats_it = miner_stats_map.find(miner_addr);
    }

    MinerStats* miner_stats = &miner_stats_it->second;

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
        this->total_round_effort[COIN_SYMBOL].pow += diff;

        worker_stats->interval_valid_shares++;
        worker_stats->interval_effort += diff;

        // no need the interval shares, as its easy (fast) to calculate from
        // workers on frontend (for miner)
        miner_stats->round_effort[COIN_SYMBOL].pow += diff;
    }
    stats_map_mutex.unlock();
}

bool StatsManager::ClosePoWRound(std::string_view chain,
                                 const BlockSubmission& submission,
                                 bool accepted, double fee)
{
    std::scoped_lock stats_lock(stats_map_mutex, *rc_mutex);

    double total_effort = total_round_effort[COIN_SYMBOL].pow;
    double block_reward = (double)submission.blockReward / 1e8;
    uint32_t height = submission.height;
    int command_amount = 0;

    // time needs to be same as the time the balance is appended to,
    // so no staking time will be missed
    AppendPoSBalances(chain, submission.timeMs);

    redisAppendCommand(rc, "HSET " COIN_SYMBOL ":round_effort_pow total 0");    command_amount++;

    if (!accepted)
    {
        block_reward = 0;
    }

    for (std::pair<std::string_view, MinerStats> miner_entry : miner_stats_map)
    {
        std::string_view miner_addr = miner_entry.first;
        MinerStats* miner_stats = &miner_entry.second;
        double miner_effort = miner_stats->round_effort[COIN_SYMBOL].pow;

        double miner_share = miner_effort / total_effort;
        double miner_reward = block_reward * miner_share * (1 - fee);

        redisAppendCommand(
            rc,
            "LSET round_entries_pow:%b 0 {\"height\":%d,\"effort\":%f,"
            "\"share\":%f,\"reward\":%f}",
            miner_addr.data(), miner_addr.length(), height,
            miner_stats->round_effort[COIN_SYMBOL].pow, miner_share,
            miner_reward);
        command_amount++;

        // reset for next round
        redisAppendCommand(rc, "LPUSH round_entries_pow:%b {\"effort\":0}",
                           miner_addr.data(), miner_addr.length());

        command_amount++;

        redisAppendCommand(rc,
                           "TS.INCRBY "
                           "%b:balance:%b %f"
                           " TIMESTAMP %" PRId64,
                           chain.data(), chain.length(), miner_addr.data(),
                           miner_addr.length(), miner_reward,
                           submission.timeMs);
        command_amount++;

        miner_stats->round_effort[COIN_SYMBOL].pow = 0;

        Logger::Log(LogType::Debug, LogField::Redis,
                    "Round: %d, miner: %.*s, effort: %f, share: %f, reward: "
                    "%f, total effort: %f",
                    height, miner_addr.length(), miner_addr.data(),
                    miner_effort, miner_share, miner_reward, total_effort);
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

    // reset for next round
    total_round_effort[COIN_SYMBOL].pow = 0;

    return true;
}

bool StatsManager::AppendPoSBalances(std::string_view chain, int64_t from_ms)
{
    redisReply* reply;
    redisAppendCommand(rc, "TS.MGET FILTER type=balance coin=%.*",
                       chain.length(), chain.data());

    if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
    {
        Logger::Log(LogType::Critical, LogField::StatsManager,
                    "Failed to get balances: %s\n", rc->errstr);
        return false;
    }

    // assert type = arr (2)
    int command_count = 0;
    for (int i = 0; i < reply->elements; i++)
    {
        // key is 2d array of the following arrays: key name, (empty array -
        // labels), values (ar ray of tuples)
        redisReply* key = reply->element[i];
        auto keyName = std::string(key[0].element[0]->str);
        std::string minerAddr =
            keyName.substr(keyName.find_last_of(':') + 1, keyName.size());

        // skip key name + null value
        redisReply* values = key[0].element[2];

        int64_t timestamp = values->element[0]->integer;
        char* value_str = values->element[1]->str;
        int64_t value = std::stoll(value_str);

        // time staked * value staked
        int64_t posPoints = (from_ms - timestamp) * value;

        total_round_effort[chain].pos += posPoints;

        miner_stats_map[minerAddr].round_effort[chain].pos += posPoints;

        // update pos effort
        redisAppendCommand(
            rc, "LSET round_entries_pos:%s 0 {\"effort\":% " PRIi64 "}",
            minerAddr.c_str(),
            miner_stats_map[minerAddr].round_effort[chain].pos);
        command_count++;

        std::cout << "timestamp: " << timestamp << " value: " << value << " "
                  << value_str << std::endl;
    }

    redisAppendCommand(rc, "SET %.*s:round_effort_pos %" PRIi64, chain.length(),
                       chain.data(), total_round_effort[chain].pos);
    command_count++;
    // redisAppendCommand(rc, "HSET " COIN_SYMBOL ":round_effort_pos total 0");

    // free balances reply
    freeReplyObject(reply);

    for (int i = 0; i < command_count; i++)
    {
        redisReply* reply;
        if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to append pos points: %s\n", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
    }

    return true;
}

double StatsManager::GetTotalEffort(std::string_view chain)
{
    std::lock_guard stats_lock(stats_map_mutex);
    return total_round_effort[COIN_SYMBOL].pow;
}

// TODO: idea: since writing all shares on all pbaas is expensive every 10 sec,
// just write to primary and to side chains write every 5 mins