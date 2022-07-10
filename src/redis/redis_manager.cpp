#include "redis_manager.hpp"

RedisManager::RedisManager(std::string ip, int port)
    : rc(redisConnect(ip.c_str(), port))
{
    using namespace std::string_view_literals;

    if (rc->err)
    {
        Logger::Log(LogType::Critical, LogField::Redis,
                    "Failed to connect to redis: {}", rc->errstr);
        exit(EXIT_FAILURE);
    }

    AppendTsCreate("round_effort_percent"sv, 0, {});
    AppendTsCreate("worker_count"sv, 0, {});
    AppendTsCreate("miner_count"sv, 0, {});
    AppendTsCreate("hashrate:pool:IL"sv,
                   StatsManager::hashrate_ttl_seconds * 1000,
                   {{"type"sv, "pool-hashrate"sv}});

    GetReplies();
}

// void RedisManager::ClosePoSRound(int64_t roundStartMs, int64_t foundTimeMs,
//                                  int64_t reward, uint32_t height,
//                                  const double totalEffort, const double fee)
// {
//     std::scoped_lock lock(rc_mutex);

//     redisReply *hashReply, *balReply;

//     if (totalEffort == 0)
//     {
//         Logger::Log(LogType::Critical, LogField::Redis,
//                     "Total effort is 0, cannot close PoS round!");
//         return;
//     }

//     AppendCommand("TS.MRANGE %" PRId64 " %" PRId64
//                   " FILTER type=balance coin=" COIN_SYMBOL,
//                   roundStartMs, foundTimeMs);

//     // reply for TS.MRANGE
//     if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
//     {
//         Logger::Log(LogType::Critical, LogField::Redis,
//                     "Failed to get balances: {}", rc->errstr);
//         return;
//     }

//     const size_t miner_count = hashReply->elements / 2;
//     Logger::Log(LogType::Info, LogField::Redis,
//                 "Closing round with {} balance changes", miner_count);

//     // separate append and get to pipeline (1 send, 1 recv!)
//     for (int i = 0; i < hashReply->elements; i += 2)
//     {
//         char *miner = hashReply->element[i]->str;
//         double minerEffort = std::stod(hashReply->element[i + 1]->str);

//         double minerShare = minerEffort / totalEffort;
//         int64_t minerReward = (int64_t)(reward * minerShare);
//         minerReward -= minerReward * fee;  // substract fee

//         AppendCommand("TS.INCRBY " COIN_SYMBOL ":immature-balance:%s %"
//         PRId64
//                       " TIMESTAMP %" PRId64,
//                       miner, minerReward, foundTimeMs);

//         // round format: height:effort_percent:reward
//         // NX = only new
//         AppendCommand("ZADD " COIN_SYMBOL ":rounds:%s NX %" PRId64
//                       " %u:%f:%" PRId64,
//                       miner, foundTimeMs, height, minerShare, minerReward);

//         Logger::Log(LogType::Debug, LogField::Redis,
//                     "Round: {}, miner: {}, effort: {}, share: {}, reward: "
//                     "{}, total effort: {}",
//                     height, miner, minerEffort, minerShare, minerReward,
//                     totalEffort);
//     }

//     for (int i = 0; i < hashReply->elements; i += 2)
//     {
//         // reply for TS.INCRBY
//         if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
//         {
//             Logger::Log(LogType::Critical, LogField::Redis,
//                         "Failed to increase balance: {}", rc->errstr);
//         }
//         freeReplyObject(balReply);

//         if (redisGetReply(rc, (void **)&balReply) != REDIS_OK)
//         {
//             Logger::Log(LogType::Critical, LogField::Redis,
//                         "Failed to set round earning stats: {}", rc->errstr);
//         }

//         freeReplyObject(balReply);
//     }
//     freeReplyObject(hashReply);  // free HGETALL reply
// }

bool RedisManager::DoesAddressExist(std::string_view addrOrId,
                                    std::string &valid_addr)
{
    std::scoped_lock lock(rc_mutex);

    redisReply *reply;
    bool isId = addrOrId.ends_with("@");

    if (isId)
    {
        reply =
            (redisReply *)redisCommand(rc, "TS.QUERYINDEX id=%b type=hashrate",
                                       addrOrId.data(), addrOrId.size());
    }
    else
    {
        reply = (redisReply *)redisCommand(
            rc, "TS.QUERYINDEX address=%b type=hashrate", addrOrId.data(),
            addrOrId.size());
    }

    bool exists = reply && reply->elements == 1;
    bool res = false;
    if (exists)
    {
        valid_addr =
            reply->element[0]->str + sizeof(COIN_SYMBOL ":hashrate:") - 1;
        res = true;
    }
    freeReplyObject(reply);
    return res;
}

uint32_t RedisManager::GetBlockNumber()
{
    std::scoped_lock lock(rc_mutex);

    // fix
    auto *reply =
        (redisReply *)redisCommand(rc, "GET " COIN_SYMBOL ":block_number");

    if (!reply)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to get block count: {}", rc->errstr);
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
    std::scoped_lock lock(rc_mutex);

    auto *reply = (redisReply *)redisCommand(rc, "INCR block_number");

    if (!reply)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to increment block count: {}", rc->errstr);
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    return true;
}

int RedisManager::AddNetworkHr(std::string_view chain, int64_t time, double hr)
{
    std::scoped_lock lock(rc_mutex);

    redisReply *reply;
    std::string key = fmt::format("{}:{}", chain, "network_difficulty");
    AppendTsAdd(std::string_view(key), time, hr);
    return GetReplies();
}

// void RedisManager::UpdatePoS(uint64_t from, uint64_t maturity)
// {
//     std::scoped_lock lock(rc_mutex);

//     redisReply *reply;
//     AppendCommand("TS.MRANGE %" PRId64 " %" PRId64
//                   " FILTER type=balance coin=" COIN_SYMBOL,
//                   from, maturity);

//     if (redisGetReply(rc, (void **)&reply) != REDIS_OK)
//     {
//         Logger::Log(LogType::Error, LogField::Redis,
//                     "Failed to get balances: {}", rc->errstr);
//         return;
//     }

//     // assert type = arr (2)
//     for (int i = 0; i < reply->elements; i++)
//     {
//         // key is 2d array of the following arrays: key name, (empty array -
//         // labels), values (array of tuples)
//         redisReply *key = reply->element[i];
//         char *keyName = key[0].element[0]->str;

//         // skip key name + null value
//         redisReply *values = key[0].element[2];

//         for (int j = 0; j < values->elements; j++)
//         {
//             int64_t timestamp = values->element[j]->element[0]->integer;
//             char *value_str = values->element[j]->element[1]->str;
//             int64_t value = std::stoll(value_str);
//             std::cout << "timestamp: " << timestamp << " value: " << value
//                       << " " << value_str << std::endl;
//         }
//     }
// }

bool RedisManager::UpdateImmatureRewards(std::string_view chain,
                                         int64_t rewardTime, bool matured)
{
    std::scoped_lock lock(rc_mutex);

    const auto *immatureReply = (redisReply *)redisCommand(
        rc, "TS.MRANGE %" PRId64 " %" PRId64 " FILTER type=immature-balance",
        rewardTime, rewardTime);

    if (!immatureReply)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to add set mature rewards: {}", rc->errstr);
        return false;
    }

    double matured_funds = 0;
    // either mature everything or nothing
    {
        RedisTransaction update_rewards_tx(this);

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
                AppendCommand("TS.INCRBY %b:mature-balance:%b %f", chain.data(),
                              chain.size(), minerAddr.data(), minerAddr.size(),
                              minerReward);
            }

            // if a block has been orphaned only remove the immature
            AppendCommand("TS.INCRBY %b:immature-balance:%b -%f", chain.data(),
                          chain.size(), minerAddr.data(), minerAddr.size(),
                          minerReward);
        }
    }

    Logger::Log(LogType::Info, LogField::Redis, "{} funds have matured!",
                matured_funds);

    return GetReplies();
}

void RedisManager::AppendTsCreate(
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

    AppendCommand(
        "TS.CREATE"
        " %b"
        " RETENTION %d"          // time to live
        " ENCODING COMPRESSED"   // very data efficient
        " DUPLICATE_POLICY MIN"  // min round
        " LABELS %s",
        key_name.data(), key_name.size(), retention, labels_str.c_str());
}

void RedisManager::AppendTsAdd(std::string_view key_name, int64_t time,
                               double value)
{
    AppendCommand("TS.ADD %b %" PRId64 " %f", key_name.data(), key_name.size(),
                  time, value);
}

bool RedisManager::AddWorker(std::string_view address,
                             std::string_view worker_full,
                             std::string_view idTag, int64_t curtime,
                             bool newWorker, bool newMiner)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction add_worker_tx(this);

        // 100% new, as we loaded all existing miners
        if (newMiner)
        {
            auto chain = std::string_view(COIN_SYMBOL);
            auto balance_keys = {"immature-balance"sv, "mature-balance"sv};
            for (std::string_view key_type : balance_keys)
            {
                auto key = fmt::format("{}:{}:{}", chain, key_type, address);
                AppendTsCreate(key, 0,
                               {{"type"sv, key_type},
                                {"address"sv, address},
                                {"id"sv, idTag}});
            }

            AppendTsCreate(fmt::format("worker-count:{}", address),
                           StatsManager::hashrate_ttl_seconds * 1000,
                           {{"type"sv, "worker-count"sv}});

            // reset all indexes of new miner
            AppendCommand("ZADD solver-index:join-time %f %b", (double)curtime,
                          address.data(), address.size());

            for (auto index :
                 {"worker-count"sv, "hashrate"sv, "mature-balance"sv})
            {
                AppendCommand("ZADD solver-index:%b 0 %b", index.data(),
                              index.size(), address.data(), address.size());
            }

            AppendCommand("RPUSH round_entries:pow:%b {\"effort:\":0}",
                          address.data(), address.size());

            AppendCreateStatsTs(address, idTag, "miner"sv);
        }

        if (newWorker)
        {
            AppendCreateStatsTs(worker_full, idTag, "worker"sv);
        }
        AppendUpdateWorkerCount(address, 1);
    }

    return GetReplies();
}

void RedisManager::AppendUpdateWorkerCount(std::string_view address, int amount)
{
    AppendCommand("ZINCRBY solver-index:worker-count %d %b", amount,
                  address.data(), address.size());

    AppendCommand("TS.INCRBY worker-count:%b %d", address.data(),
                  address.size(), amount);
}
bool RedisManager::PopWorker(std::string_view address)
{
    std::scoped_lock lock(rc_mutex);

    AppendUpdateWorkerCount(address, -1);

    return GetReplies();
}

void RedisManager::AppendHset(std::string_view key, std::string_view field,
                              std::string_view val)
{
    AppendCommand("HSET %b %b %b", key.data(), key.size(), field.data(),
                  field.size(), val.data(), val.size());
}

bool RedisManager::hset(std::string_view key, std::string_view field,
                        std::string_view val)
{
    AppendHset(key, field, val);
    return GetReplies();
}

int64_t RedisManager::hgeti(std::string_view key, std::string_view field)
{
    auto reply = (redisReply *)redisCommand(
        rc, "HGET %b %b", key.data(), key.size(), field.data(), field.size());
    int64_t val = 0;
    if (reply && reply->type == REDIS_REPLY_STRING)
    {
        // returns 0 if failed
        val = std::strtoll(reply->str, nullptr, 10);
    }
    freeReplyObject(reply);
    return val;
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

bool RedisManager::AddMinerShares(std::string_view chain,
                                  const BlockSubmission *submission,
                                  const std::vector<RoundShare> &miner_shares)
{
    std::scoped_lock lock(rc_mutex);

    uint32_t height = submission->height;

    // time needs to be same as the time the balance is appended to,
    // so no staking time will be missed
    // AppendPoSBalances(chain, submission.timeMs);

    // redis transaction, so either all balances are added or none
    {
        RedisTransaction close_round_tx(this);

        for (const auto &miner_share : miner_shares)
        {
            AppendCommand(
                "LSET round_entries:pow:%b 0 "
                "{\"height\":%d,\"effort\":%f,\"share\":%f,\"reward\":%" PRId64 "}",
                miner_share.address.data(), miner_share.address.size(), height,
                miner_share.effort, miner_share.share, miner_share.reward);

            // reset for next round
            AppendCommand("LPUSH round_entries:pow:%b {\"effort\":0}",
                          miner_share.address.data(),
                          miner_share.address.size());

            AppendCommand(
                "TS.INCRBY "
                "%b:immature-balance:%b %" PRId64
                " TIMESTAMP %" PRId64,
                chain.data(), chain.size(), miner_share.address.data(),
                miner_share.address.size(), miner_share.reward,
                submission->timeMs);

            AppendCommand("ZINCRBY solver-index:balance %" PRId64 " %b",
                          miner_share.reward, miner_share.address.data(),
                          miner_share.address.size());
        }
    }

    return GetReplies();
}

void RedisManager::AppendCreateStatsTs(std::string_view addrOrWorker,
                                       std::string_view id,
                                       std::string_view prefix)
{
    using namespace std::literals;

    std::string_view address = addrOrWorker.substr(0, ADDRESS_LEN);

    for (auto key_type : {"hashrate"sv, "hashrate:average"sv, "shares:valid"sv,
                          "shares:stale"sv, "shares:invalid"sv})
    {
        auto key = fmt::format("{}:{}:{}", key_type, prefix, addrOrWorker);
        AppendTsCreate(key, StatsManager::hashrate_ttl_seconds * 1000,
                       {{"type", key_type},
                        {"prefix", prefix},
                        {"address", address},
                        {"id", id}});
    }
}