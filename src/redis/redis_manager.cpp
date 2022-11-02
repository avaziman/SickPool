#include "redis_manager.hpp"

#include "payment_manager.hpp"

using enum Prefix;

redis_unique_ptr_context RedisManager::rc_unique;
redisContext *RedisManager::rc;
std::mutex RedisManager::rc_mutex;

RedisManager::RedisManager(const std::string &ip, const CoinConfig *conf, int db_index)
    : 
      conf(conf),
      key_names(conf->symbol)
{
    RedisManager::rc_unique = redis_unique_ptr_context(
        redisConnect(ip.c_str(), conf->redis.redis_port), redisFree);
    RedisManager::rc = rc_unique.get();

    Command({"SELECT", std::to_string(db_index)});
    if (rc->err)
    {
        logger.Log<LogType::Critical>("Failed to connect to redis: {}",
                                      rc->errstr);
        // redisFree(rc);
        throw std::runtime_error("Failed to connect to redis");
    }
}
void RedisManager::Init()
{
    using namespace std::string_view_literals;

    const int64_t hashrate_ttl_ms = conf->redis.hashrate_ttl_seconds * 1000;
    const int64_t hashrate_interval_ms =
        conf->stats.hashrate_interval_seconds * 1000;

    AppendCommand({"TS.CREATE"sv, key_names.hashrate_pool, "RETENTION"sv,
                   std::to_string(hashrate_ttl_ms)});

    for (const auto &[key_name, key_compact_name] :
         {std::make_pair(key_names.hashrate_pool,
                         key_names.hashrate_pool_compact),
          std::make_pair(key_names.hashrate_network,
                         key_names.hashrate_network_compact),
          std::make_pair(key_names.worker_count_pool,
                         key_names.worker_countp_compact),
          std::make_pair(key_names.miner_count, key_names.miner_count_compact),
          std::make_pair(key_names.difficulty, key_names.difficulty_compact)})
    {
        AppendCommand({"TS.CREATE"sv, key_name, "RETENTION"sv,
                       std::to_string(hashrate_ttl_ms)});

        // we want x7 duration
        AppendCommand({"TS.CREATE"sv, key_compact_name, "RETENTION"sv,
                       std::to_string(hashrate_ttl_ms * 7)});

        // we want 12 times less points to fit in 7 days (1 hr instead of 5min)
        AppendCommand({"TS.CREATERULE"sv, key_name, key_compact_name,
                       "AGGREGATION"sv, "AVG"sv,
                       std::to_string(hashrate_interval_ms * 12)});
    }

    // mined blocks, only compact needed.
    AppendCommand({"TS.CREATE"sv, key_names.block_number_compact, "RETENTION"sv,
                   std::to_string(hashrate_ttl_ms * 7)});

    // effort percent
    auto key_name = key_names.block_effort_percent;
    auto key_compact_name = key_names.block_effort_percent_compact;
    AppendCommand({"TS.CREATE"sv, key_name, "RETENTION"sv,
                   std::to_string(hashrate_ttl_ms * 7)});

    AppendCommand({"TS.CREATE"sv, key_compact_name, "RETENTION"sv,
                   std::to_string(hashrate_ttl_ms * 7)});

    AppendCommand({"TS.CREATERULE"sv, key_name, key_compact_name,
                   "AGGREGATION"sv, "AVG"sv,
                   std::to_string(hashrate_interval_ms * 12)});

    if (!GetReplies())
    {
        logger.Log<LogType::Critical>(
            "Failed to connect to add pool timeserieses", rc->errstr);
        throw std::runtime_error("Failed to connect to add pool timeserieses");
    }
}

RedisManager::~RedisManager() { }

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

    // AppendTsAdd(key_name.hashrate_network, curtime_ms, net_est_hr);

    // AppendSetMinerEffort(chain, EnumName<ESTIMATED_EFFORT>(), "pow",
    //                      target_diff);
    return GetReplies();
}

bool RedisManager::UpdateImmatureRewards(std::string_view chain,
                                         uint32_t block_num,
                                         int64_t matured_time, bool matured)
{
    using namespace std::string_view_literals;
    using namespace std::string_literals;
    std::scoped_lock lock(rc_mutex);

    auto reply = Command(
        {"HGETALL"sv, Format({key_names.reward_immature, std::to_string(block_num)})});

    int64_t matured_funds = 0;
    // either mature everything or nothing
    {
        RedisTransaction update_rewards_tx(this);

        for (int i = 0; i < reply->elements; i += 2)
        {
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

            std::string mature_rewards_key =
                Format({key_names.reward_mature, addr});
            AppendCommand({"LPUSH"sv, mature_rewards_key,
                           std::string_view((char *)round_share_block_num,
                                            sizeof(round_share_block_num))});

            AppendCommand({"LTRIM"sv, mature_rewards_key, "0"sv,
                           std::to_string(ROUND_SHARES_LIMIT)});

            std::string reward_str = std::to_string(miner_share->reward);
            std::string_view reward_sv(reward_str);

            // // used for payments
            // AppendCommand({"HSET"sv, fmt::format("mature-rewards:{}", addr),
            //                std::to_string(block_num), reward_sv});

            AppendCommand(
                {"ZINCRBY"sv, key_names.solver_index_mature, reward_sv, addr});

            std::string_view solver_key =
                fmt::format("{}:{}", key_names.solver, addr);

            AppendCommand({"HINCRBY"sv, solver_key, EnumName<MATURE_BALANCE>(),
                           reward_sv});
            AppendCommand({"HINCRBY"sv, solver_key,
                           EnumName<IMMATURE_BALANCE>(),
                           fmt::format("-{}", reward_sv)});

            // for payment manager...
            AppendCommand({"PUBLISH", key_names.block_mature_channel, "OK"});

            matured_funds += miner_share->reward;
        }

        // we pushed it to mature round shares list
        AppendCommand(
            {"UNLINK"sv, Format({key_names.reward_immature, std::to_string(block_num)})});
    }

    logger.Log<LogType::Info>("{} funds have matured!", matured_funds);
    return GetReplies();
}

bool RedisManager::GetAddresses(std::vector<std::string> &addresses)
{
    auto cmd = Command({"SMEMBERS", key_names.address_map});

    addresses.reserve(cmd->elements);
    for (int i = 0; i < cmd->elements; i++)
    {
        std::string key(cmd->element[i]->str, cmd->element[i]->str);

        addresses.push_back(key);
    }
    return cmd.get();
}

bool RedisManager::LoadUnpaidRewards(payees_info_t &rewards,
                                     const std::vector<std::string> &addresses)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    {
        RedisTransaction tx(this);

        rewards.reserve(addresses.size());

        for (const std::string &addr : addresses)
        {
            AppendCommand({"HMGET"sv, Format({key_names.solver, addr}),
                           EnumName<MATURE_BALANCE>(),
                           EnumName<PAYOUT_THRESHOLD>(),
                           EnumName<PAYOUT_FEELESS>()});
            // rewards.emplace_back(addr, 0);
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
        rewards[i].first = addresses[i];
        rewards[i].second = PayeeInfo{
            .amount =
                std::strtoll(reply->element[i]->element[0]->str, nullptr, 10),
            .settings = {.threshold = std::strtoll(
                             reply->element[i]->element[1]->str, nullptr, 10),
                         .pool_block_only =
                             reply->element[i]->element[2]->str[0] == '1'}};
        // const auto script_reply = reply->element[i]->element[2];
        // const auto script_len = script_reply->len;

        // rewards[i].second.script_pub_key.resize(script_len / 2);
        // Unhexlify(rewards[i].second.script_pub_key.data(), script_reply->str,
        //           script_len);
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

        AppendCommand(
            {"HSET"sv, key_names.address_map, addr_lowercase, address});

        // create worker count ts
        std::string worker_count_key =
            Format({key_names.worker_count, address});

        AppendTsCreate(worker_count_key, "miner"sv, EnumName<WORKER_COUNT>(),
                       addr_lowercase, id_tag,
                       conf->redis.hashrate_ttl_seconds * 1000);

        // reset worker count
        AppendTsAdd(worker_count_key, curtime, 0);

        std::string curtime_str = std::to_string(curtime);
        // don't override if it exists
        AppendCommand({"ZADD"sv, key_names.solver_index_jointime, "NX",
                       curtime_str, address});

        // reset all indexes of new miner
        AppendCommand(
            {"ZADD"sv, key_names.solver_index_worker_count, "0", address});
        AppendCommand({"ZADD"sv, key_names.solver_index_mature, "0", address});
        AppendCommand(
            {"ZADD"sv, key_names.solver_index_hashrate, "0", address});

        auto solver_key = Format({key_names.solver, address});
        AppendCommand({"HSET"sv, solver_key, EnumName<START_TIME>(),
                       curtime_str, EnumName<PAYOUT_THRESHOLD>(),
                       std::to_string(PaymentManager::minimum_payout_threshold),
                       EnumName<PAYOUT_FEELESS>(), "0"sv, EnumName<HASHRATE>(),
                       "0"sv, EnumName<MATURE_BALANCE>(), "0"sv,
                       EnumName<IMMATURE_BALANCE>(), "0"sv,
                       EnumName<WORKER_COUNT>(), "0"sv});

        if (id_tag != "null")
        {
            // AppendCommand(
            //     {"HSET"sv, key_names.address_map, id_tag, address});
            // AppendCommand(
            //     {"HSET"sv, solver_key, PrefixKey<IDENTITY>(), id_tag});
        }
        
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

    AppendCommand(
        {"ZADD"sv, key_names.solver_index_worker_count, address, amount_str});

    AppendCommand({"HSET"sv, fmt::format("{}:{}", key_names.solver, address),
                   EnumName<WORKER_COUNT>(), amount_str});

    AppendCommand({"TS.ADD"sv, Format({key_names.worker_count, address}),
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

    auto retention = conf->redis.hashrate_ttl_seconds * 1000;

    for (std::string_view key_type :
         {key_names.hashrate, key_names.hashrate_average,
          key_names.shares_valid, key_names.shares_stale,
          key_names.shares_invalid})
    {
        auto key = fmt::format("{}:{}:{}", key_type, prefix, addrOrWorker);
        AppendTsCreate(key, prefix, key_type, addr_lowercase_sv, id, retention);
    }
}