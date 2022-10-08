#include "redis_manager.hpp"

RedisManager::RedisManager(const std::string &ip, int port, int hashrate_ttl_ms)
    : rc(redisConnect(ip.c_str(), port))
{
    using namespace std::string_view_literals;

    if (rc->err)
    {
        Logger::Log(LogType::Critical, LogField::Redis,
                    "Failed to connect to redis: {}", rc->errstr);
        redisFree(rc);
        throw std::runtime_error("Failed to connect to redis");
    }

    // AppendCommand({
    //     "round_effort_percent"sv,
    // });
    auto pool_worker_count_key = fmt::format("{}:{}", PrefixKey<Prefix::WORKER_COUNT>(), "pool");
    AppendCommand({"TS.CREATE"sv, pool_worker_count_key});

    AppendTsAdd(pool_worker_count_key, GetCurrentTimeMs(), 0);

    AppendCommand(
        {"TS.CREATE"sv, fmt::format("{}:{}", PrefixKey<Prefix::MINER_COUNT>(), "pool")});
    AppendCommand({"TS.CREATE"sv, fmt::format("{}:{}", PrefixKey<Prefix::HASHRATE>(), "pool"),
                   "RETENTION"sv, std::to_string(hashrate_ttl_ms), "LABELS"sv,
                   "type"sv, "pool-hashrate"sv});

    if (!GetReplies())
    {
        Logger::Log(LogType::Critical, LogField::Redis,
                    "Failed to connect to add pool timeserieses", rc->errstr);
        throw std::runtime_error("Failed to connect to add pool timeserieses");
    }
}

RedisManager::~RedisManager() { redisFree(rc); }

// TODO: think of smt else
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

bool RedisManager::SetNewBlockStats(std::string_view chain, int64_t curtime_ms,
                                    double net_est_hr, double target_diff)
{
    std::scoped_lock lock(rc_mutex);

    AppendAddNetworkHr(chain, curtime_ms, net_est_hr);
    AppendSetMinerEffort(chain, PrefixKey<Prefix::ESTIMATED_EFFORT>(), "pow",
                         target_diff);
    return GetReplies();
}

void RedisManager::AppendAddNetworkHr(std::string_view chain, int64_t time,
                                      double hr)
{
    std::string key =
        fmt::format("{}:{}", chain, PrefixKey<Prefix::NETWORK_HASHRATE>());
    AppendTsAdd(std::string_view(key), time, hr);
}

bool RedisManager::UpdateImmatureRewards(std::string_view chain,
                                         uint32_t block_num,
                                         int64_t matured_time, bool matured)
{
    using namespace std::string_view_literals;
    using namespace std::string_literals;
    std::scoped_lock lock(rc_mutex);

    auto reply = Command(
        {"HGETALL"sv, fmt::format("{}:{}", PrefixKey<Prefix::IMMATURE_REWARD>(), block_num)});

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
            // if there are sub chains add chain:

            if (!matured)
            {
                miner_share->reward = 0;
            }

            // used to show round share statistics
            uint8_t
                round_share_block_num[sizeof(miner_share) + sizeof(block_num)];
            memcpy(round_share_block_num, miner_share, sizeof(miner_share));
            memcpy(round_share_block_num + sizeof(miner_share), &block_num,
                   sizeof(block_num));

            std::string mature_shares_key =
                fmt::format("mature-shares:{}", addr);
            AppendCommand({"LPUSH"sv, mature_shares_key,
                           std::string_view((char *)round_share_block_num,
                                            sizeof(round_share_block_num))});

            AppendCommand({"LTRIM"sv, mature_shares_key, "0"sv,
                           std::to_string(ROUND_SHARES_LIMIT)});

            std::string reward_str = std::to_string(miner_share->reward);
            std::string_view reward_sv(reward_str);

            // // used for payments
            // AppendCommand({"HSET"sv, fmt::format("mature-rewards:{}", addr),
            //                std::to_string(block_num), reward_sv});

            AppendCommand({"ZINCRBY"sv,
                           fmt::format("solver-index:{}", PrefixKey<Prefix::MATURE_BALANCE>()),
                           reward_sv, addr});

            auto solver_key = fmt::format("solver:{}", addr);
            AppendCommand(
                {"HINCRBY"sv, solver_key, PrefixKey<Prefix::MATURE_BALANCE>(), reward_sv});
            AppendCommand({"HINCRBY"sv, solver_key, PrefixKey<Prefix::IMMATURE_BALANCE>(),
                           fmt::format("-{}", reward_sv)});

            matured_funds += miner_share->reward;
        }

        // we pushed it to mature round shares list
        AppendCommand(
            {"UNLINK"sv, fmt::format("{}:{}", PrefixKey<Prefix::IMMATURE_REWARD>(), block_num)});
    }

    Logger::Log(LogType::Info, LogField::Redis, "{} funds have matured!",
                matured_funds);
    return GetReplies();
}

bool RedisManager::LoadUnpaidRewards(
    std::vector<std::pair<std::string, PayeeInfo>> &rewards,
    const efforts_map_t &efforts, std::mutex *efforts_mutex)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction tx(this);
        std::scoped_lock lock(*efforts_mutex);

        rewards.reserve(efforts.size());

        for (const auto &[addr, _] : efforts)
        {
            AppendCommand({"HMGET"sv, fmt::format("solver:{}", addr),
                           PrefixKey<Prefix::MATURE_BALANCE>(), PrefixKey<Prefix::PAYOUT_THRESHOLD>(),
                           PrefixKey<Prefix::SCRIPT_PUB_KEY>()});
            rewards.emplace_back(addr, 0);
        }
    }

    redis_unique_ptr reply;
    if (!GetReplies(&reply)) return false;

    for (int i = 0; i < reply->elements; i++)
    {
        if (reply->element[i]->elements != 3 ||
            reply->element[i]->element[0]->type != REDIS_REPLY_STRING ||
            reply->element[i]->element[1]->type != REDIS_REPLY_STRING ||
            reply->element[i]->element[2]->type != REDIS_REPLY_STRING)
        {
            continue;
        }

        rewards[i].second.amount =
            std::strtoll(reply->element[i]->element[0]->str, nullptr, 10);

        rewards[i].second.settings.threshold =
            std::strtoll(reply->element[i]->element[1]->str, nullptr, 10);

        const auto script_reply = reply->element[i]->element[2];
        const auto script_len = script_reply->len;

        rewards[i].second.script_pub_key.resize(script_len / 2);
        Unhexlify(rewards[i].second.script_pub_key.data(), script_reply->str,
                  script_len);
    }
    return true;
}

void RedisManager::AppendTsAdd(std::string_view key_name, int64_t time,
                               double value)
{
    using namespace std::string_view_literals;
    AppendCommand(
        {"TS.ADD"sv, key_name, std::to_string(time), fmt::format("{}", value)});
}

void RedisManager::AppendTsCreate(std::string_view key, std::string_view prefix,
                                  std::string_view type,
                                  std::string_view address, std::string_view id,
                                  uint64_t retention_ms)
{
    using namespace std::string_view_literals;
    AppendCommand({"TS.CREATE"sv, key, "RETENTION"sv,
                   std::to_string(retention_ms), "LABELS"sv, "type"sv, type,
                   "prefix"sv, prefix, "address"sv, address, "identity"sv, id});
}

bool RedisManager::AddNewMiner(std::string_view address,
                               std::string_view addr_lowercase,
                               std::string_view worker_full,
                               std::string_view id_tag,
                               std::string_view script_pub_key, int64_t curtime)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction add_worker_tx(this);

        // 100% new, as we loaded all existing
        // miners
        auto chain = std::string_view(COIN_SYMBOL);

        AppendCommand({"HSET"sv, PrefixKey<Prefix::ADDRESS_MAP>(), addr_lowercase, address});

        // create worker count ts
        std::string worker_count_key =
            fmt::format("{}:{}", PrefixKey<Prefix::WORKER_COUNT>(), address);
        AppendTsCreate(worker_count_key, "miner"sv, PrefixKey<Prefix::WORKER_COUNT>(),
                       addr_lowercase, id_tag,
                       StatsManager::hashrate_ttl_seconds * 1000);

        // reset worker count
        AppendTsAdd(worker_count_key, curtime, 0);

        std::string curtime_str = std::to_string(curtime);
        // don't override if it exists
        AppendCommand({"ZADD"sv, fmt::format("solver-index:{}", PrefixKey<Prefix::JOIN_TIME>()),
                       "NX", curtime_str, address});

        // reset all indexes of new miner
        for (std::string_view index :
             {PrefixKey<Prefix::WORKER_COUNT>(), PrefixKey<Prefix::HASHRATE>(), PrefixKey<Prefix::MATURE_BALANCE>()})
        {
            AppendCommand({"ZADD"sv, fmt::format("solver-index:{}", index), "0",
                           address});
        }

        auto solver_key = fmt::format("solver:{}", address);
        AppendCommand({"HSET"sv, solver_key, PrefixKey<Prefix::JOIN_TIME>(), curtime_str,
                       PrefixKey<Prefix::SCRIPT_PUB_KEY>(), script_pub_key, PrefixKey<Prefix::PAYOUT_THRESHOLD>(),
                       std::to_string(PaymentManager::minimum_payout_threshold),
                       PrefixKey<Prefix::HASHRATE>(), "0"sv, PrefixKey<Prefix::MATURE_BALANCE>(), "0"sv,
                       PrefixKey<Prefix::IMMATURE_BALANCE>(), "0"sv, PrefixKey<Prefix::WORKER_COUNT>(), "0"sv});

        if (id_tag != "null")
        {
            AppendCommand({"HSET"sv, PrefixKey<Prefix::ADDRESS_MAP>(), id_tag, address});
            AppendCommand({"HSET"sv, solver_key, PrefixKey<Prefix::IDENTITY>(), id_tag});
        }
        // set round effort to 0
        AppendSetMinerEffort(chain, address, "pow", 0);
        AppendSetMinerEffort(chain, address, "pos", 0);

        // to be accessible by lowercase addr
        AppendCreateStatsTs(address, id_tag, "miner"sv, addr_lowercase);
    }

    return GetReplies();
}

bool RedisManager::AddNewWorker(std::string_view address,
                                std::string_view address_lowercase,
                                std::string_view worker_full,
                                std::string_view id_tag)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction add_worker_tx(this);

        AppendCreateStatsTs(worker_full, id_tag, "worker"sv, address_lowercase);
    }

    return GetReplies();
}

void RedisManager::AppendUpdateWorkerCount(std::string_view address, int amount,
                                           int64_t update_time_ms)
{
    using namespace std::string_view_literals;
    std::string amount_str = std::to_string(amount);

    AppendCommand({"ZADD"sv, fmt::format("solver-index:{}", PrefixKey<Prefix::WORKER_COUNT>()),
                   address, amount_str});

    AppendCommand({"HSET"sv, fmt::format("solver:{}", address),
                   PrefixKey<Prefix::WORKER_COUNT>(), amount_str});

    AppendCommand({"TS.ADD"sv, fmt::format("{}:{}", PrefixKey<Prefix::WORKER_COUNT>(), address),
                   std::to_string(update_time_ms), amount_str});
}

// bool RedisManager::PopWorker(std::string_view address)
// {
//     std::scoped_lock lock(rc_mutex);

//     AppendUpdateWorkerCount(address, -1);

//     return GetReplies();
// }

void RedisManager::AppendHset(std::string_view key, std::string_view field,
                              std::string_view val)
{
    AppendCommand({"HSET", key, field, val});
}

bool RedisManager::hset(std::string_view key, std::string_view field,
                        std::string_view val)
{
    AppendHset(key, field, val);
    return GetReplies();
}

std::string RedisManager::hget(std::string_view key, std::string_view field)
{
    using namespace std::string_view_literals;
    auto reply = Command({"HGET"sv, key, field});

    if (reply && reply->type == REDIS_REPLY_STRING)
    {
        // returns 0 if failed
        return std::string(reply->str, reply->len);
    }
    return std::string();
}

void RedisManager::AppendCreateStatsTs(std::string_view addrOrWorker,
                                       std::string_view id,
                                       std::string_view prefix,
                                       std::string_view addr_lowercase_sv)
{
    using namespace std::literals;

    std::string_view address = addrOrWorker.substr(0, ADDRESS_LEN);
    std::string addr_lowercase;

    if (addr_lowercase_sv.empty())
    {
        addr_lowercase = address;
        std::transform(addr_lowercase.begin(), addr_lowercase.end(),
                       addr_lowercase.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        addr_lowercase_sv = std::string_view(addr_lowercase);
    }

    auto retention = StatsManager::hashrate_ttl_seconds * 1000;

    for (auto key_type : {PrefixKey<Prefix::HASHRATE>(), "hashrate:average"sv, "shares:valid"sv,
                          "shares:stale"sv, "shares:invalid"sv})
    {
        auto key = fmt::format("{}:{}:{}", key_type, prefix, addrOrWorker);
        AppendTsCreate(key, prefix, key_type, addr_lowercase_sv, id, retention);
    }
}