#include "./stats_manager.hpp"

StatsManager::StatsManager(redisContext* rc, std::mutex* rc_mutex,
                           int hr_interval, int effort_interval,
                           int avg_hr_interval)
    : rc(rc),
      rc_mutex(rc_mutex),
      hashrate_interval_seconds(hr_interval),
      effort_interval_seconds(effort_interval),
      average_hashrate_interval_seconds(avg_hr_interval)
{
    LoadCurrentRound();
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
        now - (now % hashrate_interval_seconds) + hashrate_interval_seconds;

    while (true)
    {
        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Next stats update in: %d", next_effort_update);

        std::time_t next_update = std::min(next_hr_update, next_effort_update);

        bool should_update_hr = false;
        bool should_update_effort = false;
        redisReply* remove_avg_reply;

        if (next_update == next_hr_update)
        {
            std::scoped_lock lock(*rc_mutex, stats_map_mutex);
            should_update_hr = true;

            std::time_t remove_hr_time =
                (next_hr_update - average_hashrate_interval_seconds) * 1000;
            remove_avg_reply = (redisReply*)redisCommand(
                rc,
                "TS.MRANGE %" PRIi64 " %" PRIi64 " FILTER type=miner-hashrate",
                remove_hr_time, remove_hr_time);

            for (int i = 0; i < remove_avg_reply->elements; i++)
            {
                const redisReply* address_rep =
                    remove_avg_reply->element[i]->element[0];
                std::string_view address_sv(address_rep->str, address_rep->len);
                address_sv = address_sv.substr(address_sv.size() - ADDRESS_LEN,
                                               address_sv.size());

                if (remove_avg_reply->element[i]->element[2]->elements == 0)
                {
                    continue;
                }

                const double remove_hr =
                    std::strtod(remove_avg_reply->element[i]
                                    ->element[2]
                                    ->element[0]
                                    ->element[1]
                                    ->str,
                                nullptr);
                miner_stats_map[address_sv].average_hashrate_sum -= remove_hr;
            }
            freeReplyObject(remove_avg_reply);

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

        double pool_hr = 0;
        int command_count = 0;
        uint64_t toMs = next_update * 1000;

        // either update everyone or no one
        redisAppendCommand(rc, "MULTI");
        command_count++;

        Logger::Log(LogType::Info, LogField::StatsManager,
                    "Updating stats for %d workers, is_interval: %d.",
                    worker_stats_map.size(), should_update_hr);

        {
            std::scoped_lock lock(*rc_mutex, stats_map_mutex);
            for (auto& [worker, ws] : worker_stats_map)
            {
                if (should_update_hr)
                {
                    const double interval_hashrate =
                        ws.interval_effort / (double)effort_interval_seconds;

                    ws.ResetInterval();

                    // worker hashrate update
                    redisAppendCommand(
                        rc, "TS.ADD hashrate:worker:%b %" PRIu64 " %f",
                        worker.data(), worker.length(), toMs,
                        interval_hashrate);

                    // ws.average_hashrate_sum += interval_hashrate;
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
                    const double interval_hashrate =
                        miner_ws.interval_effort / hashrate_interval_seconds;

                    miner_ws.ResetInterval();

                    // interval stats update
                    // miner hashrate
                    redisAppendCommand(
                        rc, "TS.ADD hashrate:miner:%b %" PRIu64 " %f",
                        miner_addr.data(), miner_addr.length(), toMs,
                        interval_hashrate);

                    // sorted miner hashrate set
                    redisAppendCommand(rc, "ZADD solver-index:hashrate %f %b",
                                       interval_hashrate, miner_addr.data(),
                                       miner_addr.length());
                    // pool hashrate
                    pool_hr += interval_hashrate;

                    // miner average hashrate
                    miner_ws.average_hashrate_sum += interval_hashrate;
                    const double avg_hr = miner_ws.average_hashrate_sum /
                                          (average_hashrate_interval_seconds /
                                           hashrate_interval_seconds);
                    redisAppendCommand(
                        rc, "TS.ADD hashrate:miner-average:%b %" PRIi64 " %f",
                        miner_addr.data(), miner_addr.size(), toMs, avg_hr);

                    command_count += 3;
                }
            }

            if (should_update_effort)
            {
                // TODO: make for each side chain every 5 mins
                redisAppendCommand(
                    rc, "HSET " COIN_SYMBOL ":round_effort_pow total %f",
                    total_round_effort[COIN_SYMBOL].pow);
                command_count++;
            }

            if (should_update_hr)
            {
                redisAppendCommand(rc, "TS.ADD pool_hashrate %" PRIu64 " %f",
                                   toMs, pool_hr);
                command_count++;
            }

            redisAppendCommand(rc, "EXEC");
            command_count++;

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
        }

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
    auto reply = (redisReply*)redisCommand(rc, "HGET " COIN_SYMBOL
                                               ":round_effort_pow "
                                               "total");

    if (reply->type == REDIS_REPLY_r)
    {
        // if the key doesn't exist, don't create a key and so it will be 0
        total_round_effort[COIN_SYMBOL].pow =
            std::stod(std::string(reply->str, reply->len));
    }
    freeReplyObject(reply);
    Logger::Log(LogType::Info, LogField::StatsManager,
                "Loaded pow round effort of %f",
                total_round_effort[COIN_SYMBOL].pow);

    auto miners_reply =
        (redisReply*)redisCommand(rc, "ZRANGE solver-index:join-time 0 -1");

    // either load everyone or no one
    redisAppendCommand(rc, "MULTI");

    size_t miner_count = miners_reply->elements;
    for (int i = 0; i < miner_count; i++)
    {
        std::string_view miner_addr(miners_reply->element[i]->str,
                                    miners_reply->element[i]->len);
        redisAppendCommand(rc, "LINDEX round_entries_pow:%b 0",
                           miner_addr.data(), miner_addr.size());

        // reset worker count
        redisAppendCommand(rc, "ZADD solver-index:worker-count 0 %b",
                           miner_addr.data(), miner_addr.size());
    }

    redisAppendCommand(rc, "EXEC");

    for (int i = 0; i < (miner_count * 2) + 2; i++)
    {
        if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to queue round_entries_pow get");
            exit(EXIT_FAILURE);
        }

        // we need to read the last response
        if (i != (miner_count * 2) + 1)
        {
            freeReplyObject(reply);
        }
    }

    for (int i = 0; i < miner_count; i++)
    {
        auto miner = std::string_view(miners_reply->element[i]->str,
                                      miners_reply->element[i]->len);
        const double minerEffort = std::strtod(
            reply->element[i]->str + sizeof("{\"effort\":") - 1, nullptr);
        miner_stats_map[miner].round_effort[COIN_SYMBOL].pow = minerEffort;

        Logger::Log(LogType::Debug, LogField::StatsManager,
                    "Loaded %.*s effort of %f", miner.size(), miner.data(),
                    minerEffort);
    }

    freeReplyObject(reply);
    // don't free the miners_reply as it will invalidate the string_views
    // freeReplyObject(miners_reply);
    // TODO: load each miner
}

// load the stats for the current round, and create entries if they don't exist
std::unordered_map<std::string_view, MinerStats>::iterator
StatsManager::LoadMinerStats(std::string_view miner_addr)
{
    std::scoped_lock lock(*rc_mutex);
    auto it = miner_stats_map.try_emplace(miner_addr).first;

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
    std::scoped_lock lock(stats_map_mutex);
    WorkerStats* worker_stats = &worker_stats_map[worker_full];
    auto miner_stats_it = miner_stats_map.find(miner_addr);

    if (miner_stats_it == miner_stats_map.end())
    {
        // stats_map_mutex already locked
        LoadMinerStats(miner_addr);

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
        miner_stats->interval_effort += diff;
    }
}

bool StatsManager::AddWorker(std::string_view address,
                             std::string_view worker_full,
                             std::string_view identity, std::time_t curtime)
{
    std::scoped_lock stats_db_lock(stats_map_mutex, *rc_mutex);

    // (will be true on restart too, as we don't reload worker stats)
    bool newWorker =
        worker_stats_map.find(worker_full) == worker_stats_map.end();
    bool newMiner = miner_stats_map.find(address) == miner_stats_map.end();
    int command_count = 0;

    // 100% new, as we loaded all existing miners
    if (newMiner)
    {
        redisAppendCommand(rc, "ZADD solver-index:join-time %f %b",
                           (double)curtime, address.data(), address.size());
        // reset all other stats
        redisAppendCommand(rc, "ZADD solver-index:worker-count 0 %b",
                           address.data(), address.size());
        redisAppendCommand(rc, "ZADD solver-index:hashrate 0 %b",
                           address.data(), address.size());
        redisAppendCommand(rc, "ZADD solver-index:balance 0 %b", address.data(),
                           address.size());
        command_count += 4;
    }

    // worker has been disconnected and came back, add him again
    if (newWorker || worker_stats_map[worker_full].connection_count == 0)
    {
        redisAppendCommand(
            rc,
            "TS.CREATE " COIN_SYMBOL
            ":hashrate:worker:%b"
            " RETENTION " xstr(HASHRATE_RETENTION)  // time to live
            " ENCODING COMPRESSED"                  // very data efficient
            " LABELS coin " COIN_SYMBOL
            " type worker_hashrate server IL address %b worker %b identity %b",
            worker_full.data(), worker_full.size(), address.data(),
            address.size(), worker_full.data(), worker_full.size(),
            identity.data(), identity.size());
        command_count++;

        redisAppendCommand(rc, "ZINCRBY solver-index:worker-count 1 %b",
                           address.data(), address.size());
        command_count++;
    }
    worker_stats_map[worker_full].connection_count++;

    redisReply* reply;
    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to add worker: %s\n", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
    }
    return true;
}

void StatsManager::PopWorker(std::string_view worker_full,
                             std::string_view address)
{
    std::scoped_lock stats_lock(stats_map_mutex);

    int con_count = (--worker_stats_map[worker_full].connection_count);
    // dont remove from the umap in case they join back, so their progress is
    // saved
    if (con_count == 0)
    {
        std::scoped_lock redis_lock(*rc_mutex);

        auto reply = redisCommand(rc, "ZINCRBY solver-index:worker-count -1 %b",
                                  address.data(), address.size());
        freeReplyObject(reply);
    }
}

bool StatsManager::ClosePoWRound(std::string_view chain,
                                 const BlockSubmission& submission, double fee)
{
    // redis mutex already locked
    std::scoped_lock stats_lock(stats_map_mutex);

    double total_effort = total_round_effort[COIN_SYMBOL].pow;
    double block_reward = (double)submission.blockReward / 1e8;
    uint32_t height = submission.height;
    int command_count = 0;

    // reset for next round
    total_round_effort[COIN_SYMBOL].pow = 0;

    // time needs to be same as the time the balance is appended to,
    // so no staking time will be missed
    AppendPoSBalances(chain, submission.timeMs);

    // redis transaction, so either all balances are added or none
    redisAppendCommand(rc, "MULTI");
    command_count++;

    redisAppendCommand(rc, "HSET " COIN_SYMBOL ":round_effort_pow total 0");
    command_count++;

    for (auto& [miner_addr, miner_stats] : miner_stats_map)
    {
        double miner_effort = miner_stats.round_effort[chain].pow;

        double miner_share = miner_effort / total_effort;
        double miner_reward = block_reward * miner_share * (1 - fee);

        redisAppendCommand(
            rc,
            "LSET round_entries_pow:%b 0 {\"height\":%d,\"effort\":%f,"
            "\"share\":%f,\"reward\":%f}",
            miner_addr.data(), miner_addr.length(), height,
            miner_stats.round_effort[COIN_SYMBOL].pow, miner_share,
            miner_reward);
        command_count++;

        // reset for next round
        redisAppendCommand(rc, "LPUSH round_entries_pow:%b {\"effort\":0}",
                           miner_addr.data(), miner_addr.length());
        command_count++;

        redisAppendCommand(rc,
                           "TS.INCRBY "
                           "%b:balance:%b %f"
                           " TIMESTAMP %" PRId64,
                           chain.data(), chain.length(), miner_addr.data(),
                           miner_addr.length(), miner_reward,
                           submission.timeMs);
        command_count++;

        redisAppendCommand(rc, "ZINCRBY solver-index:balance %f %b",
                           miner_reward, miner_addr.data(),
                           miner_addr.length());
        command_count++;

        miner_stats.round_effort[chain].pow = 0;

        Logger::Log(LogType::Debug, LogField::Redis,
                    "Round: %d, miner: %.*s, effort: %f, share: %f, reward: "
                    "%f, total effort: %f",
                    height, miner_addr.length(), miner_addr.data(),
                    miner_effort, miner_share, miner_reward, total_effort);
    }

    redisAppendCommand(rc, "EXEC");
    command_count++;

    redisReply* reply;
    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to close PoW round: %s\n", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
    }

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
        // labels), values (array of tuples)
        const redisReply* key = reply->element[i];
        auto keyName = std::string(key[0].element[0]->str);
        std::string minerAddr =
            keyName.substr(keyName.find_last_of(':') + 1, keyName.size());

        // skip key name + null value of labels
        const redisReply* values = key[0].element[2];

        int64_t timestamp = values->element[0]->integer;
        const char* value_str = values->element[1]->str;
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

    redisAppendCommand(rc, "SET %b:round_effort_pos %" PRIi64, chain.data(),
                       chain.length(), total_round_effort[chain].pos);
    command_count++;
    // redisAppendCommand(rc, "HSET " COIN_SYMBOL ":round_effort_pos total 0");

    // free balances reply
    freeReplyObject(reply);

    for (int i = 0; i < command_count; i++)
    {
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
    return total_round_effort[chain].pow;
}

// TODO: idea: since writing all shares on all pbaas is expensive every 10 sec,
// just write to primary and to side chains write every 5 mins
// TODO: on restart, restart all worker counts 0, remove worker index
// TODO: make sure address ts is created before ts.adding to it