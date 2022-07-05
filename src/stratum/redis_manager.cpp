#include "redis_manager.hpp"

RedisManager::RedisManager(std::string ip, int port)
{
    using namespace std::string_view_literals;
    rc = redisConnect(ip.c_str(), port);

    if (rc->err)
    {
        Logger::Log(LogType::Critical, LogField::Redis,
                    "Failed to connect to redis: %s", rc->errstr);
        exit(EXIT_FAILURE);
    }

    redisReply *reply;
    int command_count = 0;

    command_count += AppendTsCreate("round_effort_percent"sv, 0, {});
    command_count += AppendTsCreate("worker_count"sv, 0, {});
    command_count += AppendTsCreate("miner_count"sv, 0, {});
    command_count += AppendTsCreate("hashrate:pool:IL"sv,
                                    StatsManager::hashrate_ttl_seconds * 1000,
                                    {{"type"sv, "pool-hashrate"sv}});

    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to create (%d) startup keys: %s", i,
                        rc->errstr);
            exit(EXIT_FAILURE);
        }
        freeReplyObject(reply);
    }
}

bool RedisManager::UpdateBlockConfirmations(std::string_view block_id,
                                            int32_t confirmations)
{
    // redis bitfield uses be so gotta swap em
    return redisCommand(rc, "BITFIELD block:%b SET i32 0 %d", block_id.data(),
                        block_id.size(), bswap_32(confirmations)) == nullptr;
}

bool RedisManager::AddBlockSubmission(const BlockSubmission *submission)
{
    int command_count = 0;
    uint32_t block_id = submission->number;
    auto chain =
        std::string((char *)submission->chain, sizeof(submission->chain));

    redisAppendCommand(rc, "MULTI");
    command_count++;

    // serialize the block submission to save space and net
    // bandwidth, as the indexes are added manually anyway no need for hash
    redisAppendCommand(rc, "SET block:%u %b", block_id, submission,
                       sizeof(BlockSubmission));
    command_count++;

    /* sortable indexes */
    // block no. and block time will always be same order
    // so only one index is required to sort by either of them
    // (block num value is smaller)
    redisAppendCommand(rc, "ZADD block-index:number %f %u",
                       (double)submission->number, block_id);

    redisAppendCommand(rc, "ZADD block-index:reward %f %u",
                       (double)submission->blockReward, block_id);

    redisAppendCommand(rc, "ZADD block-index:difficulty %f %u",
                       submission->difficulty, block_id);

    redisAppendCommand(rc, "ZADD block-index:effort %f %u",
                       submission->effortPercent, block_id);

    redisAppendCommand(rc, "ZADD block-index:duration %f %u",
                       (double)submission->durationMs, block_id);
    command_count += 5;
    // /* non-sortable indexes */
    redisAppendCommand(rc, "SADD block-index:chain:%s %u", chain.c_str(),
                       block_id);
    command_count++;

    redisAppendCommand(rc, "SADD block-index:type:PoW %u", block_id);
    command_count++;

    redisAppendCommand(rc, "SADD block-index:solver:%b %u", submission->miner,
                       sizeof(submission->miner), block_id);
    command_count++;

    command_count += AppendTsAdd(chain + ":round_effort_percent",
                                 submission->timeMs, submission->effortPercent);

    redisAppendCommand(rc, "EXEC");
    command_count++;

    redisReply *reply;
    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to add block submission and indexes: %s",
                        rc->errstr);
            return false;
        }
        freeReplyObject(reply);
    }
    return true;
}

bool RedisManager::SetEstimatedNeededEffort(std::string_view chain,
                                            double effort)
{
    redisReply *reply;
    redisAppendCommand(rc, "HSET %b:round_effort_pow estimated %f",
                       chain.data(), chain.size(), effort);

    if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
    {
        Logger::Log(LogType::Critical, LogField::Redis,
                    "Failed to set estimated needed effort: %s", rc->errstr);
        return false;
    }

    freeReplyObject(reply);
    return true;
}

void RedisManager::ClosePoSRound(int64_t roundStartMs, int64_t foundTimeMs,
                                 int64_t reward, uint32_t height,
                                 const double totalEffort, const double fee)
{
    redisReply *hashReply, *balReply;

    if (totalEffort == 0)
    {
        Logger::Log(LogType::Critical, LogField::Redis,
                    "Total effort is 0, cannot close PoS round!");
        return;
    }

    redisAppendCommand(rc,
                       "TS.MRANGE %" PRId64 " %" PRId64
                       " FILTER type=balance coin=" COIN_SYMBOL,
                       roundStartMs, foundTimeMs);

    // reply for TS.MRANGE
    if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
    {
        Logger::Log(LogType::Critical, LogField::Redis,
                    "Failed to get balances: %s", rc->errstr);
        return;
    }

    Logger::Log(LogType::Info, LogField::Redis,
                "Closing round with %d balance changes",
                hashReply->elements / 2);

    // separate append and get to pipeline (1 send, 1 recv!)
    for (int i = 0; i < hashReply->elements; i += 2)
    {
        char *miner = hashReply->element[i]->str;
        double minerEffort = std::stod(hashReply->element[i + 1]->str);

        double minerShare = minerEffort / totalEffort;
        int64_t minerReward = (int64_t)(reward * minerShare);
        minerReward -= minerReward * fee;  // substract fee

        redisAppendCommand(rc,
                           "TS.INCRBY " COIN_SYMBOL
                           ":immature-balance:%s %" PRId64
                           " TIMESTAMP %" PRId64,
                           miner, minerReward, foundTimeMs);

        // round format: height:effort_percent:reward
        // NX = only new
        redisAppendCommand(
            rc, "ZADD " COIN_SYMBOL ":rounds:%s NX %" PRId64 " %u:%f:%" PRId64,
            miner, foundTimeMs, height, minerShare, minerReward);

        Logger::Log(LogType::Debug, LogField::Redis,
                    "Round: %d, miner: %s, effort: %f, share: %f, reward: "
                    "%" PRId64 ", total effort: %f",
                    height, miner, minerEffort, minerShare, minerReward,
                    totalEffort);
    }

    for (int i = 0; i < hashReply->elements; i += 2)
    {
        // reply for TS.INCRBY
        if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to increase balance: %s", rc->errstr);
        }
        freeReplyObject(balReply);

        if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::Redis,
                        "Failed to set round earning stats: %s", rc->errstr);
        }

        freeReplyObject(balReply);
    }
    freeReplyObject(hashReply);  // free HGETALL reply
}

bool RedisManager::DoesAddressExist(std::string_view addrOrId,
                                    std::string &valid_addr)
{
    redisReply *reply;
    bool isId = addrOrId.ends_with("@");

    if (isId)
    {
        redisAppendCommand(rc, "TS.QUERYINDEX id=%b", addrOrId.data(),
                           addrOrId.size());
    }
    else
    {
        redisAppendCommand(rc, "TS.QUERYINDEX address=%b", addrOrId.data(),
                           addrOrId.size());
    }

    if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to check if address/identity exists: %s",
                    rc->errstr);
        return false;
    }

    bool exists = reply->elements == 1;
    if (exists)
    {
        valid_addr =
            reply->element[0]->str + sizeof(COIN_SYMBOL ":balance:") - 1;
        freeReplyObject(reply);
        return true;
    }
    freeReplyObject(reply);

    return false;
}

int64_t RedisManager::GetLastRoundTimePow()
{
    redisReply *reply;
    redisAppendCommand(rc, "TS.GET " COIN_SYMBOL ":round_effort");

    if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to get current round time stamp: %s", rc->errstr);
        return 0;
    }
    else if (reply->elements != 2)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to get current round time stamp (wrong response)");
        freeReplyObject(reply);
        return 0;
    }

    int64_t roundStart = reply->element[0]->integer;
    freeReplyObject(reply);

    return roundStart;
}

uint32_t RedisManager::GetBlockNumber()
{
    // fix
    auto *reply =
        (redisReply *)redisCommand(rc, "GET " COIN_SYMBOL ":block_number");

    if (!reply)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to get block count: %s", rc->errstr);
        return 0;
    }
    else if (reply->type != REDIS_REPLY_INTEGER)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to get block count (wrong response)");
        freeReplyObject(reply);
        return 0;
    }

    uint32_t res = reply->integer;
    freeReplyObject(reply);

    return res;
}

bool RedisManager::IncrBlockCount()
{
    auto *reply = (redisReply *)redisCommand(rc, "INCR block_number");

    if (!reply)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to increment block count: %s", rc->errstr);
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    return true;
}

int RedisManager::AddNetworkHr(std::string_view chain, int64_t time, double hr)
{
    redisReply *reply;
    std::string key = fmt::format("{}:{}", chain, "network_difficulty");
    AppendTsAdd(std::string_view(key), time, hr);
    if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to add network difficulty: %s", rc->errstr);
        freeReplyObject(reply);
        return 1;
    }
    freeReplyObject(reply);
    return 0;
}

void RedisManager::UpdatePoS(uint64_t from, uint64_t maturity)
{
    redisReply *reply;
    redisAppendCommand(rc,
                       "TS.MRANGE %" PRId64 " %" PRId64
                       " FILTER type=balance coin=" COIN_SYMBOL,
                       from, maturity);

    if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to get balances: %s", rc->errstr);
        return;
    }

    // assert type = arr (2)
    for (int i = 0; i < reply->elements; i++)
    {
        // key is 2d array of the following arrays: key name, (empty array -
        // labels), values (array of tuples)
        redisReply *key = reply->element[i];
        char *keyName = key[0].element[0]->str;

        // skip key name + null value
        redisReply *values = key[0].element[2];

        for (int j = 0; j < values->elements; j++)
        {
            int64_t timestamp = values->element[j]->element[0]->integer;
            char *value_str = values->element[j]->element[1]->str;
            int64_t value = std::stoll(value_str);
            std::cout << "timestamp: " << timestamp << " value: " << value
                      << " " << value_str << std::endl;
        }
    }
}

bool RedisManager::UpdateImmatureRewards(std::string_view chain,
                                         int64_t rewardTime, bool matured)
{
    const auto *immatureReply = (redisReply *)redisCommand(
        rc, "TS.MRANGE %" PRId64 " %" PRId64 " FILTER type=immature-balance",
        rewardTime, rewardTime);

    if (!immatureReply)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to add set mature rewards: %s", rc->errstr);
        return false;
    }

    double matured_funds = 0;
    int command_count = 0;
    // either mature everything or nothing
    redisAppendCommand(rc, "MULTI");
    command_count++;

    for (int i = 0; i < immatureReply->elements; i++)
    {
        const redisReply *replyEntry = immatureReply->element[i];
        const auto minerAddr = std::string_view(
            strrchr(replyEntry->element[0]->str, ':'), ADDRESS_LEN);
        double minerReward = 0;

        if (replyEntry->element[2]->elements &&
            replyEntry->element[2]->element[0]->elements)
        {
            const redisReply *rewardReply =
                replyEntry->element[2]->element[0]->element[1];
            minerReward = strtod(rewardReply->str, nullptr);
            matured_funds += minerReward;
        }

        if (matured)
        {
            redisAppendCommand(rc, "TS.INCRBY %b:mature-balance:%b %f",
                               chain.data(), chain.size(), minerAddr.data(),
                               minerAddr.size(), minerReward);
            command_count++;
        }

        // if a block has been orphaned only remove the immature
        redisAppendCommand(rc, "TS.INCRBY %b:immature-balance:%b -%f",
                           chain.data(), chain.size(), minerAddr.data(),
                           minerAddr.size(), minerReward);
        command_count++;
    }

    redisAppendCommand(rc, "EXEC");
    command_count++;

    redisReply *reply;
    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Error, LogField::Redis,
                        "Failed to update immature rewards of time %" PRId64
                        ", error: %s",
                        rewardTime, rc->errstr);
        }
        freeReplyObject(reply);
    }

    Logger::Log(LogType::Info, LogField::Redis, "%f funds have matured!",
                matured_funds);

    return true;
}

int RedisManager::AppendTsCreate(
    std::string_view key_name, int retention,
    std::initializer_list<std::tuple<std::string_view, std::string_view>>
        labels)
{
    std::string labels_str = "";
    for (const auto &[key, val] : labels)
    {
        labels_str.append(key);
        labels_str.append(" ");
        labels_str.append(val);
        labels_str.append(" ");
    }

    if (labels.size())
    {
        labels_str.pop_back();  // remove last space
    }

    redisAppendCommand(rc,
                       "TS.CREATE"
                       " %b"
                       " RETENTION %d"          // time to live
                       " ENCODING COMPRESSED"   // very data efficient
                       " DUPLICATE_POLICY MIN"  // min round
                       " LABELS %s",
                       key_name.data(), key_name.size(), retention,
                       labels_str.c_str());
    return 1;
}

int RedisManager::AppendTsAdd(std::string_view key_name, int64_t time,
                              double value)
{
    redisAppendCommand(rc, "TS.ADD %b %" PRId64 " %f", key_name.data(),
                       key_name.size(), time, value);
    return 1;
}
int RedisManager::AppendStatsUpdate(std::string_view addr,
                                    std::string_view prefix,
                                    int64_t update_time_ms, double hr,
                                    const WorkerStats &ws)
{
    // miner hashrate
    std::string key = fmt::format("{}:{}:{}", "hashrate", prefix, addr);
    AppendTsAdd(key, update_time_ms, hr);

    // average hashrate
    key = fmt::format("{}:{}:{}", "hashrate:average", prefix, addr);
    AppendTsAdd(key, update_time_ms, ws.average_hashrate);

    // shares
    key = fmt::format("{}:{}:{}", "shares:valid", prefix, addr);
    AppendTsAdd(key, update_time_ms, ws.interval_valid_shares);

    key = fmt::format("{}:{}:{}", "shares:invalid", prefix, addr);
    AppendTsAdd(key, update_time_ms, ws.interval_invalid_shares);

    key = fmt::format("{}:{}:{}", "shares:stale", prefix, addr);
    AppendTsAdd(key, update_time_ms, ws.interval_stale_shares);
    return 5;
}

bool RedisManager::AddWorker(std::string_view address,
                             std::string_view worker_full,
                             std::string_view idTag, int64_t curtime,
                             bool newWorker, bool newMiner)
{
    using namespace std::string_view_literals;

    int command_count = 0;

    redisAppendCommand(rc, "MULTI");
    command_count++;

    // 100% new, as we loaded all existing miners
    if (newMiner)
    {
        auto chain = std::string_view(COIN_SYMBOL);
        auto balance_keys = {"immature-balance"sv, "mature-balance"sv};
        for (std::string_view key_type : balance_keys)
        {
            auto key = fmt::format("{}:{}:{}", chain, key_type, address);
            command_count += AppendTsCreate(key, 0,
                                            {{"type"sv, key_type},
                                             {"address"sv, address},
                                             {"id"sv, idTag}});
        }

        command_count +=
            AppendTsCreate(fmt::format("worker-count:{}", address),
                           StatsManager::hashrate_ttl_seconds * 1000,
                           {{"type"sv, "worker-count"sv}});

        // reset all indexes of new miner
        redisAppendCommand(rc, "ZADD solver-index:join-time %f %b",
                           (double)curtime, address.data(), address.size());

        for (auto index : {"worker-count"sv, "hashrate"sv, "mature-balance"sv})
        {
            redisAppendCommand(rc, "ZADD solver-index:%b 0 %b", index.data(),
                               index.size(), address.data(), address.size());
        }

        redisAppendCommand(rc, "RPUSH round_entries:pow:%b {\"effort:\":0}",
                           address.data(), address.size());

        command_count += 5;

        command_count += AppendCreateStatsTs(address, idTag, "miner"sv);
    }

    // worker has been disconnected and came back, add him again
    if (newWorker)
    {
        command_count += AppendCreateStatsTs(worker_full, idTag, "worker"sv);
    }
    command_count += AppendUpdateWorkerCount(address, 1);

    redisAppendCommand(rc, "EXEC");
    command_count++;

    redisReply *reply;
    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to add worker: %s\n", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
    }
    return true;
}

int RedisManager::AppendUpdateWorkerCount(std::string_view address, int amount)
{
    redisAppendCommand(rc, "ZINCRBY solver-index:worker-count %d %b", amount,
                       address.data(), address.size());

    redisAppendCommand(rc, "TS.INCRBY worker-count:%b %d", address.data(),
                       address.size(), amount);
    return 2;
}
bool RedisManager::PopWorker(std::string_view address)
{
    int command_count = AppendUpdateWorkerCount(address, -1);

    redisReply *reply;
    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to pop worker: %s\n", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
    }
    return true;
}

bool RedisManager::hset(std::string_view key, std::string_view field,
                        std::string_view val)
{
    auto reply = (redisReply *)redisCommand(
        rc, "HSET %b %b %b", key.data(), key.size(), field.data(), field.size(),
        val.data(), val.size());

    return reply;
}

int RedisManager::hgeti(std::string_view key, std::string_view field)
{
    auto reply = (redisReply *)redisCommand(
        rc, "HGET %b %b", key.data(), key.size(), field.data(), key.size());
    int val = 0;
    if (reply && reply->type == REDIS_REPLY_STRING)
    {
        // returns 0 if failed
        val = std::strtoll(reply->str, nullptr, 10);
    }
    freeReplyObject(reply);
    return val;
}

bool RedisManager::SetLastRoundTimePow(std::string_view chain, int64_t val)
{
    return hset(fmt::format("round:pow:{}", chain), "start",
                std::to_string(val));
}

double RedisManager::hgetd(std::string_view key, std::string_view field)
{
    auto reply = (redisReply *)redisCommand(
        rc, "HGET %b %b", key.data(), key.size(), field.data(), field.size());
    double val = 0;
    if (reply && reply->type == REDIS_REPLY_STRING)
    {
        // returns 0 if failed
        val = std::strtod(reply->str, nullptr);
    }
    freeReplyObject(reply);
    return val;
}
bool RedisManager::LoadSolvers(
    std::unordered_map<std::string, MinerStats> &miner_stats_map,
    std::unordered_map<std::string, Round> &round_map)
{
    auto miners_reply =
        (redisReply *)redisCommand(rc, "ZRANGE solver-index:join-time 0 -1");

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
        int64_t from = (now - (now % StatsManager::hashrate_interval_seconds) -
                        StatsManager::hashrate_interval_seconds) *
                       1000;

        redisAppendCommand(
            rc,
            "TS.RANGE hashrate:miner:%b %" PRIi64 " + AGGREGATION SUM %" PRIi64,
            miner_addr.data(), miner_addr.size(), from,
            StatsManager::average_hashrate_interval_seconds * 1000);

        // reset worker count
        redisAppendCommand(rc, "ZADD solver-index:worker-count 0 %b",
                           miner_addr.data(), miner_addr.size());

        redisAppendCommand(rc, "TS.ADD worker-count:%b * 0", miner_addr.data(),
                           miner_addr.size());

        command_count += 4;
    }

    redisAppendCommand(rc, "EXEC");
    command_count++;

    redisReply *reply;
    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to queue round_entries:pow get");
            return false;
        }

        // we need to read the last response
        if (i != command_count - 1)
        {
            freeReplyObject(reply);
        }
    }

    for (int i = 0; i < miner_count; i++)
    {
        auto miner_addr = std::string(miners_reply->element[i]->str,
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
    freeReplyObject(miners_reply);
    return true;
}

bool RedisManager::ClosePoWRound(
    std::string_view chain, const BlockSubmission *submission, double fee,
    std::unordered_map<std::string, MinerStats> &miner_stats_map,
    std::unordered_map<std::string, Round> &round_map)
{
    double total_effort = round_map[COIN_SYMBOL].pow;
    double block_reward = (double)submission->blockReward / 1e8;
    uint32_t height = submission->height;
    int command_count = 0;

    round_map[COIN_SYMBOL].round_start_ms = submission->timeMs;
    SetLastRoundTimePow(COIN_SYMBOL, submission->timeMs);

    // reset for next round
    round_map[COIN_SYMBOL].pow = 0;

    // time needs to be same as the time the balance is appended to,
    // so no staking time will be missed
    // AppendPoSBalances(chain, submission.timeMs);

    // redis transaction, so either all balances are added or none
    redisAppendCommand(rc, "MULTI");
    command_count++;

    redisAppendCommand(rc, "HSET round_start " COIN_SYMBOL " %" PRIi64,
                       submission->timeMs);
    command_count++;

    redisAppendCommand(rc, "HSET " COIN_SYMBOL ":round_effort_pow total 0");
    command_count++;

    for (auto &[miner_addr, miner_stats] : miner_stats_map)
    {
        const double miner_effort = miner_stats.round_effort[chain].pow;

        const double miner_share = miner_effort / total_effort;
        const double miner_reward = block_reward * miner_share * (1 - fee);

        redisAppendCommand(
            rc,
            "LSET round_entries:pow:%b 0 {\"height\":%d,\"effort\":%f,"
            "\"share\":%f,\"reward\":%f}",
            miner_addr.data(), miner_addr.size(), height,
            miner_stats.round_effort[COIN_SYMBOL].pow, miner_share,
            miner_reward);
        command_count++;

        // reset for next round
        redisAppendCommand(rc, "LPUSH round_entries:pow:%b {\"effort\":0}",
                           miner_addr.data(), miner_addr.size());
        command_count++;

        redisAppendCommand(rc,
                           "TS.INCRBY "
                           "%b:immature-balance:%b %f"
                           " TIMESTAMP %" PRId64,
                           chain.data(), chain.length(), miner_addr.data(),
                           miner_addr.length(), miner_reward,
                           submission->timeMs);
        command_count++;

        redisAppendCommand(rc, "ZINCRBY solver-index:balance %f %b",
                           miner_reward, miner_addr.data(), miner_addr.size());
        command_count++;

        redisAppendCommand(rc, "HSET round_start " COIN_SYMBOL " %" PRIi64,
                           submission->timeMs);

        miner_stats.round_effort[chain].pow = 0;

        Logger::Log(LogType::Debug, LogField::Redis,
                    "Round id: %u, miner: %.*s, effort: %f, share: %f, reward: "
                    "%f, total effort: %f",
                    submission->number, miner_addr.size(), miner_addr.data(),
                    miner_effort, miner_share, miner_reward, total_effort);
    }

    redisAppendCommand(rc, "EXEC");
    command_count++;

    redisReply *reply;
    for (int i = 0; i < command_count; i++)
    {
        if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
        {
            Logger::Log(LogType::Critical, LogField::StatsManager,
                        "Failed to close PoW round: %s\n", rc->errstr);
            return false;
        }
        freeReplyObject(reply);
    }
    return true;
}

int RedisManager::AppendCreateStatsTs(std::string_view addrOrWorker,
                                      std::string_view id,
                                      std::string_view prefix)
{
    using namespace std::literals;

    std::string_view address = addrOrWorker.substr(0, ADDRESS_LEN);

    int command_count = 0;

    for (auto key_type : {"hashrate"sv, "hashrate:average"sv, "shares:valid"sv,
                          "shares:stale"sv, "shares:invalid"sv})
    {
        auto key = fmt::format("{}:{}:{}", key_type, prefix, addrOrWorker);
        command_count +=
            AppendTsCreate(key, StatsManager::hashrate_ttl_seconds * 1000,
                           {{"type", key_type},
                            {"prefix", prefix},
                            {"address", address},
                            {"id", id}});
    }

    return command_count;
}