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
    auto *reply = (redisReply *)redisCommand(rc, "GET block_number");

    if (!reply || reply->type != REDIS_REPLY_INTEGER)
    {
        Logger::Log(LogType::Error, LogField::Redis,
                    "Failed to get block count");
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

bool RedisManager::UpdateImmatureRewards(std::string_view chain,
                                         uint32_t block_num,
                                         int64_t matured_time, bool matured)
{
    std::scoped_lock lock(rc_mutex);

    auto reply = (redisReply *)redisCommand(rc, "HGETALL immature-rewards:%u",
                                            block_num);
    double matured_funds = 0;
    // either mature everything or nothing
    {
        RedisTransaction update_rewards_tx(this);

        for (int i = 0; i < reply->elements; i += 2)
        {
            double minerReward = 0;
            std::string_view addr(reply->element[i]->str,
                                  reply->element[i]->len);

            RoundShare *miner_share = (RoundShare *)reply->element[i + 1]->str;

            // if a block has been orphaned only remove the immature
            AppendCommand("HINCRBY %b:balance-immature %b -%" PRId64,
                          chain.data(), chain.size(), addr.data(), addr.size(),
                          miner_share->reward);

            if (!matured)
            {
                miner_share->reward = 0;
            }

            AppendCommand("HINCRBY %b:balance-mature %b %" PRId64, chain.data(),
                          chain.size(), addr.data(), addr.size(),
                          miner_share->reward);
            matured_funds += miner_share->reward;

            // AppendCommand("HSET %b:balance %b last_changed %" PRId64,
            //               chain.data(), chain.size(), addr.data(),
            //               addr.size(), matured_time);

            AppendCommand("ZINCRBY solver-index:balance %" PRId64 " %b",
                          miner_share->reward, addr.data(), addr.size());

            AppendCommand("ZADD round-entries:pow:%b %u %b", addr.data(),
                          addr.size(), block_num, miner_share,
                          sizeof(RoundShare));
        }

        AppendCommand("UNLINK immature-rewards:%u", block_num);
    }
    freeReplyObject(reply);

    Logger::Log(LogType::Info, LogField::Redis, "{} funds have matured!",
                matured_funds);
    return GetReplies();
}

bool RedisManager::TsCreate(
    std::string_view key_name, int retention,
    std::initializer_list<std::tuple<std::string_view, std::string_view>>
        labels)
{
    AppendTsCreate(key_name, retention, labels);
    return GetReplies();
}

void RedisManager::AppendTsCreate(
    std::string_view key_name, int retention,
    std::initializer_list<std::tuple<std::string_view, std::string_view>>
        labels)
{
    std::string command_str =
        "TS.CREATE"
        " %b"
        " RETENTION %d"          // time to live
        " ENCODING COMPRESSED"   // very data efficient
        " DUPLICATE_POLICY MIN"  // min round
        " LABELS";

    for (const auto &[key, val] : labels)
    {
        command_str.append(fmt::format(" {} {}", key, val));
    }

    AppendCommand(command_str.c_str(), key_name.data(), key_name.size(), retention);
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