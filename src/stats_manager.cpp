#include "./stats_manager.hpp"

StatsManager::StatsManager(redisContext* rc, std::mutex* rc_mutex,
                           int hr_interval, int effort_interval,
                           int avg_hr_interval, int hashrate_ttl)
    : rc(rc),
      rc_mutex(rc_mutex),
      hashrate_interval_seconds(hr_interval),
      effort_interval_seconds(effort_interval),
      average_hashrate_interval_seconds(avg_hr_interval),
      hashrate_ttl_seconds(hashrate_ttl)
{
    LoadCurrentRound();
    redisCommand(rc,
                 "TS.CREATE"
                 " hashrate:pool:IL"
                 " RETENTION %d"  // time to live
                 " ENCODING COMPRESSED"                  // very data efficient
                 " LABELS coin " COIN_SYMBOL
                 " type pool-hashrate server IL", hashrate_ttl * 1000);
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

    std::scoped_lock lock(*rc_mutex, stats_map_mutex);
    Logger::Log(LogType::Info, LogField::StatsManager,
                "Updating stats for %d miners, %d workers, is_interval: %d",
                miner_stats_map.size(), worker_stats_map.size(), update_hr);

    // remove oldest average hashrate before adding newest, for moving average
    if (update_hr)
    {
        int64_t remove_hr_time =
            (update_time_ms - (average_hashrate_interval_seconds * 1000));

        remove_avg_reply = (redisReply*)redisCommand(
            rc, "TS.MRANGE %" PRIi64 " %" PRIi64 " FILTER type=miner-hashrate",
            remove_hr_time, remove_hr_time);

        if (remove_avg_reply == nullptr)
        {
            return false;
        }
        for (int i = 0; i < remove_avg_reply->elements; i++)
        {
            // no hashrate in the last avg time yet
            if (remove_avg_reply->element[i]->elements < 3 ||
                remove_avg_reply->element[i]->element[2]->elements < 1 ||
                remove_avg_reply->element[i]->element[2]->element[0]->elements <
                    2)
            {
                continue;
            }

            const redisReply* address_rep =
                remove_avg_reply->element[i]->element[0];
            std::string_view address_sv(address_rep->str, address_rep->len);
            address_sv = address_sv.substr(address_sv.size() - ADDRESS_LEN,
                                           address_sv.size());

            const double remove_hr = std::strtod(remove_avg_reply->element[i]
                                                     ->element[2]
                                                     ->element[0]
                                                     ->element[1]
                                                     ->str,
                                                 nullptr);
            miner_stats_map[address_sv].average_hashrate_sum -= remove_hr;
        }
        freeReplyObject(remove_avg_reply);
    }

    // either update everyone or no one
    redisAppendCommand(rc, "MULTI");
    command_count++;

    for (auto& [worker, ws] : worker_stats_map)
    {
        if (update_hr)
        {
            const double interval_hashrate =
                ws.interval_effort / (double)hashrate_interval_seconds;
            std::string_view addr = worker.substr(0, ADDRESS_LEN);

            ws.average_hashrate_sum += interval_hashrate;

            auto& miner_stats = miner_stats_map[addr];
            miner_stats.interval_effort += ws.interval_effort;
            miner_stats.interval_valid_shares += ws.interval_valid_shares;
            miner_stats.interval_invalid_shares += ws.interval_invalid_shares;
            miner_stats.interval_stale_shares += ws.interval_stale_shares;

            command_count += AppendStatsUpdate(worker, std::string_view("worker"), update_time_ms,
                                               interval_hashrate, ws);

            ws.ResetInterval();
        }
    }
    for (auto& [miner_addr, miner_ws] : miner_stats_map)
    {
        // miner round entry
        if (update_effort)
        {
            redisAppendCommand(rc,
                               "LSET round_entries:pow:%b 0 {\"effort\":%f}",
                               miner_addr.data(), miner_addr.length(),
                               miner_ws.round_effort[COIN_SYMBOL].pow);
            command_count++;
        }

        if (update_hr)
        {
            const double interval_hashrate =
                miner_ws.interval_effort / (double)hashrate_interval_seconds;

            // pool hashrate
            pool_hr += interval_hashrate;

            // miner average hashrate
            miner_ws.average_hashrate_sum += interval_hashrate;

            command_count +=
                AppendStatsUpdate(miner_addr, std::string_view("miner"), update_time_ms,
                                  interval_hashrate, miner_ws);

            // sorted miner hashrate set (index)
            redisAppendCommand(rc, "ZADD solver-index:hashrate %f %b",
                               interval_hashrate, miner_addr.data(),
                               miner_addr.length());
            command_count++;

            miner_ws.ResetInterval();
        }
    }

    if (update_effort)
    {
        // TODO: make for each side chain every 5 mins
        redisAppendCommand(rc, "HSET " COIN_SYMBOL ":round_effort_pow total %f",
                           round_map[COIN_SYMBOL].pow);
        command_count++;
    }

    if (update_hr)
    {
        redisAppendCommand(rc, "TS.ADD hashrate:pool:IL %" PRIu64 " %f",
                           update_time_ms, pool_hr);
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
            return false;
        }
        freeReplyObject(reply);
    }

    return true;
}

inline int StatsManager::AppendStatsUpdate(std::string_view addr,
                                           std::string_view prefix,
                                           int64_t update_time_ms, double hr,
                                           const WorkerStats& ws)
{
    const double avg_hr =
        ws.average_hashrate_sum /
        ((double)average_hashrate_interval_seconds / hashrate_interval_seconds);

    // miner hashrate
    redisAppendCommand(rc, "TS.ADD hashrate:%b:%b %" PRIu64 " %f",
                       prefix.data(), prefix.size(), addr.data(), addr.length(),
                       update_time_ms, hr);
    // average hashrate
    redisAppendCommand(rc, "TS.ADD hashrate:average:%b:%b %" PRIi64 " %f",
                       prefix.data(), prefix.size(), addr.data(), addr.size(),
                       update_time_ms, avg_hr);

    // shares
    redisAppendCommand(rc, "TS.ADD shares:valid:%b:%b %" PRIu64 " %u",
                       prefix.data(), prefix.size(), addr.data(), addr.length(),
                       update_time_ms, ws.interval_valid_shares);

    redisAppendCommand(rc, "TS.ADD shares:invalid:%b:%b %" PRIu64 " %u",
                       prefix.data(), prefix.size(), addr.data(), addr.length(),
                       update_time_ms, ws.interval_invalid_shares);

    redisAppendCommand(rc, "TS.ADD shares:stale:%b:%b %" PRIu64 " %u",
                       prefix.data(), prefix.size(), addr.data(), addr.length(),
                       update_time_ms, ws.interval_stale_shares);

    return 5;
}

bool StatsManager::LoadCurrentRound()
{
    auto reply = (redisReply*)redisCommand(rc, "HGET " COIN_SYMBOL
                                               ":round_effort_pow "
                                               "total");
    if (!reply) return false;

    if (reply->type == REDIS_REPLY_STRING)
    {
        // if the key doesn't exist, don't create a key and so it will be 0
        round_map[COIN_SYMBOL].pow =
            std::stod(std::string(reply->str, reply->len));
    }

    freeReplyObject(reply);

    reply = (redisReply*)redisCommand(rc, "HGET round_start " COIN_SYMBOL);

    if (!reply) return false;

    if (reply->type == REDIS_REPLY_STRING)
    {
        round_map[COIN_SYMBOL].round_start_ms =
            std::strtoll(reply->str, nullptr, 10);
    }
    else
    {
        // round is starting now then
        round_map[COIN_SYMBOL].round_start_ms = GetCurrentTimeMs();
    }

    freeReplyObject(reply);

    Logger::Log(LogType::Info, LogField::StatsManager,
                "Loaded pow round effort of %f, round start %" PRIi64,
                round_map[COIN_SYMBOL].pow,
                round_map[COIN_SYMBOL].round_start_ms);

    auto miners_reply =
        (redisReply*)redisCommand(rc, "ZRANGE solver-index:join-time 0 -1");

    int command_count = 0;

    // either load everyone or no one
    redisAppendCommand(rc, "MULTI");
    command_count++;

    size_t miner_count = miners_reply->elements;
    for (int i = 0; i < miner_count; i++)
    {
        std::string_view miner_addr(miners_reply->element[i]->str,
                                    miners_reply->element[i]->len);

        // load round effort
        redisAppendCommand(rc, "LINDEX round_entries:pow:%b 0",
                           miner_addr.data(), miner_addr.size());

        // load sum of hashrate over last average period
        auto now = std::time(0);
        // exactly same formula as updating stats
        int64_t from = (now - (now % hashrate_interval_seconds) -
                        hashrate_interval_seconds) *
                       1000;

        redisAppendCommand(rc,
                           "TS.RANGE hashrate:miner:%b %" PRIi64
                           " + AGGREGATION SUM %" PRIi64,
                           miner_addr.data(), miner_addr.size(), from,
                           average_hashrate_interval_seconds * 1000);

        // reset worker count
        redisAppendCommand(rc, "ZADD solver-index:worker-count 0 %b",
                           miner_addr.data(), miner_addr.size());

        redisAppendCommand(rc, "TS.ADD worker-count:%b * 0", miner_addr.data(),
                           miner_addr.size());

        command_count += 4;
    }

    redisAppendCommand(rc, "EXEC");
    command_count++;

    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to queue round_entries:pow get");
            exit(EXIT_FAILURE);
        }

        // we need to read the last response
        if (i != command_count - 1)
        {
            freeReplyObject(reply);
        }
    }

    for (int i = 0; i < miner_count; i++)
    {
        auto miner_addr = std::string_view(miners_reply->element[i]->str,
                                           miners_reply->element[i]->len);

        const double miner_effort = std::strtod(
            reply->element[i * 4]->str + sizeof("{\"effort\":") - 1, nullptr);

        double sum_last_avg_interval = 0;
        if (reply->element[i * 4 + 1]->elements &&
            reply->element[i * 4 + 1]->element[0]->elements)
        {
            sum_last_avg_interval = std::strtod(
                reply->element[i * 4 + 1]->element[0]->element[1]->str,
                nullptr);
        }

        miner_stats_map[miner_addr].round_effort[COIN_SYMBOL].pow =
            miner_effort;
        miner_stats_map[miner_addr].average_hashrate_sum +=
            sum_last_avg_interval;

        Logger::Log(LogType::Debug, LogField::StatsManager,
                    "Loaded %.*s effort of %f, hashrate sum of %f",
                    miner_addr.size(), miner_addr.data(), miner_effort,
                    sum_last_avg_interval);
    }

    freeReplyObject(reply);
    // don't free the miners_reply as it will invalidate the string_views
    // freeReplyObject(miners_reply);
    // TODO: load each miner
    return true;
}

void StatsManager::AddShare(std::string_view worker_full,
                            std::string_view miner_addr, double diff)
{
    std::scoped_lock lock(stats_map_mutex);
    // both must exist, as we added in AddWorker
    WorkerStats* worker_stats = &worker_stats_map[worker_full];
    MinerStats* miner_stats = &miner_stats_map[miner_addr];

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
        this->round_map[COIN_SYMBOL].pow += diff;

        worker_stats->interval_valid_shares++;
        worker_stats->interval_effort += diff;

        // no need the interval shares, as its easy (fast) to calculate from
        // workers on frontend (for miner)
        miner_stats->round_effort[COIN_SYMBOL].pow += diff;
        // other miner stats are calculated from worker stats for efficiency
    }
}

bool StatsManager::AddWorker(std::string_view address,
                             std::string_view worker_full, int64_t curtime)
{
    std::scoped_lock stats_db_lock(stats_map_mutex, *rc_mutex);

    // (will be true on restart too, as we don't reload worker stats)
    bool newWorker = !worker_stats_map.contains(worker_full);
    bool newMiner = !miner_stats_map.contains(address);
    int command_count = 0;

    redisAppendCommand(rc, "MULTI");
    command_count++;

    // 100% new, as we loaded all existing miners
    if (newMiner)
    {
        redisAppendCommand(rc,
                           "TS.CREATE " COIN_SYMBOL
                           ":immature-balance:%b"
                           " RETENTION 0"  // never expire balance log
                           " ENCODING COMPRESSED"
                           " LABELS coin " COIN_SYMBOL " type balance",
                           address.data(), address.size());
        command_count++;

        redisAppendCommand(rc,
                       "TS.CREATE"
                       " worker-count:%b"
                       " RETENTION %d"
                       " ENCODING COMPRESSED"
                       " LABELS coin " COIN_SYMBOL " type worker-count",
                       address.data(), address.size(), hashrate_ttl_seconds * 1000);
        command_count++;

        redisAppendCommand(rc, "ZADD solver-index:join-time %f %b",
                           (double)curtime, address.data(), address.size());
        // reset all other stats
        redisAppendCommand(rc, "ZADD solver-index:worker-count 0 %b",
                           address.data(), address.size());
        redisAppendCommand(rc, "ZADD solver-index:hashrate 0 %b",
                           address.data(), address.size());
        redisAppendCommand(rc, "ZADD solver-index:balance 0 %b", address.data(),
                           address.size());

        redisAppendCommand(rc, "RPUSH round_entries:pow:%b {\"effort:\":0}",
                           address.data(), address.size());

        command_count += 5;

        command_count +=
            AppendCreateStatsTs(address, std::string_view("miner"));
    }

    // worker has been disconnected and came back, add him again
    if (newWorker || worker_stats_map[worker_full].connection_count == 0)
    {
        redisAppendCommand(rc, "ZINCRBY solver-index:worker-count 1 %b",
                           address.data(), address.size());
        command_count++;

        redisAppendCommand(rc, "TS.INCRBY worker-count:%b 1", address.data(),
                           address.size());
        command_count++;

        command_count +=
            AppendCreateStatsTs(worker_full, std::string_view("worker"));

        miner_stats_map[address].worker_count++;
    }
    worker_stats_map[worker_full].connection_count++;

    redisAppendCommand(rc, "EXEC");
    command_count++;

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

int StatsManager::AppendCreateStatsTs(std::string_view addrOrWorker,
                                      std::string_view prefix)
{
    using namespace std::literals;

    std::string_view address = addrOrWorker.substr(0, ADDRESS_LEN);

    int command_count = 0;

    for (auto i : {"hashrate"sv, "hashrate:average"sv, "shares:valid"sv,
                   "shares:stale"sv, "shares:invalid"sv})
    {
        redisAppendCommand(
            rc,
            "TS.CREATE"
            " %b:%b:%b"
            " RETENTION %d"  // time to live
            " ENCODING COMPRESSED"                  // very data efficient
            " LABELS coin " COIN_SYMBOL " type %b-%b address %b server IL",
            i.data(), i.size(), prefix.data(), prefix.size(),
            addrOrWorker.data(), addrOrWorker.size(), hashrate_ttl_seconds * 1000, prefix.data(),
            prefix.size(), i.data(), i.size(), address.data(), address.size());
        command_count++;
    }

    return command_count;
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
        std::scoped_lock redis_lock(*rc_mutex);

        redisAppendCommand(rc, "ZINCRBY solver-index:worker-count -1 %b",
                           address.data(), address.size());

        redisAppendCommand(rc, "TS.INCRBY worker-count:%b -1", address.data(),
                           address.size());

        miner_stats_map[address].worker_count--;

        redisReply* reply;
        for (int i = 0; i < 2; i++)
        {
            if (redisGetReply(rc, (void**)&reply) != REDIS_OK)
            {
                Logger::Log(LogType::Critical, LogField::StatsManager,
                            "Failed to close PoW round: %s\n", rc->errstr);
                // return false;
            }
            freeReplyObject(reply);
        }
    }
}

bool StatsManager::ClosePoWRound(std::string_view chain,
                                 const BlockSubmission& submission, double fee)
{
    // redis mutex already locked
    std::scoped_lock stats_lock(stats_map_mutex);

    double total_effort = round_map[COIN_SYMBOL].pow;
    double block_reward = (double)submission.blockReward / 1e8;
    uint32_t height = submission.height;
    int command_count = 0;

    round_map[COIN_SYMBOL].round_start_ms = submission.timeMs;
    // reset for next round
    round_map[COIN_SYMBOL].pow = 0;

    // time needs to be same as the time the balance is appended to,
    // so no staking time will be missed
    // AppendPoSBalances(chain, submission.timeMs);

    // redis transaction, so either all balances are added or none
    redisAppendCommand(rc, "MULTI");
    command_count++;

    redisAppendCommand(rc, "HSET round_start " COIN_SYMBOL " %" PRIi64,
                       submission.timeMs);
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
            "LSET round_entries:pow:%b 0 {\"height\":%d,\"effort\":%f,"
            "\"share\":%f,\"reward\":%f}",
            miner_addr.data(), miner_addr.length(), height,
            miner_stats.round_effort[COIN_SYMBOL].pow, miner_share,
            miner_reward);
        command_count++;

        // reset for next round
        redisAppendCommand(rc, "LPUSH round_entries:pow:%b {\"effort\":0}",
                           miner_addr.data(), miner_addr.length());
        command_count++;

        redisAppendCommand(rc,
                           "TS.INCRBY "
                           "%b:immature-balance:%b %f"
                           " TIMESTAMP %" PRId64,
                           chain.data(), chain.length(), miner_addr.data(),
                           miner_addr.length(), miner_reward,
                           submission.timeMs);
        command_count++;

        redisAppendCommand(rc, "ZINCRBY solver-index:balance %f %b",
                           miner_reward, miner_addr.data(),
                           miner_addr.length());
        command_count++;

        redisAppendCommand(rc, "HSET round_start " COIN_SYMBOL " %" PRIi64,
                           submission.timeMs);

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
    redisAppendCommand(rc, "TS.MGET FILTER type=mature-balance coin=%b",
                       chain.data(), chain.length());

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

        round_map[chain].pos += posPoints;

        miner_stats_map[minerAddr].round_effort[chain].pos += posPoints;

        // update pos effort
        redisAppendCommand(
            rc, "LSET round_entries:pos:%s 0 {\"effort\":% " PRIi64 "}",
            minerAddr.c_str(),
            miner_stats_map[minerAddr].round_effort[chain].pos);
        command_count++;

        std::cout << "timestamp: " << timestamp << " value: " << value << " "
                  << value_str << std::endl;
    }

    redisAppendCommand(rc, "SET %b:round_effort_pos %" PRIi64, chain.data(),
                       chain.length(), round_map[chain].pos);
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

Round StatsManager::GetChainRound(std::string_view chain)
{
    std::lock_guard stats_lock(stats_map_mutex);
    return round_map[chain];
}

// TODO: idea: since writing all shares on all pbaas is expensive every 10 sec,
// just write to primary and to side chains write every 5 mins
// TODO: maybe create block submission manager;