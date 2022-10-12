#include "redis_manager.hpp"
using enum Prefix;

void RedisManager::AppendSetMinerEffort(std::string_view chain,
                                        std::string_view miner,
                                        std::string_view type, double effort)
{
    AppendHset(fmt::format("{}:{}:{}:{}", coin_symbol, chain, type, EnumName<Prefix::ROUND_EFFORT>()), miner,
               std::to_string(effort));
}

void RedisManager::LoadCurrentRound(std::string_view chain,
                                    std::string_view type, Round *rnd)
{
    std::string round_effort_key =
        fmt::format("{}:{}:{}", chain, type, EnumName<Prefix::ROUND_EFFORT>());

    std::string total_effort_str = hget(round_effort_key, EnumName<Prefix::TOTAL_EFFORT>());

    std::string start_time_str = hget(round_effort_key, EnumName<Prefix::ROUND_START_TIME>());

    std::string estimated_effort_str =
        hget(round_effort_key, EnumName<Prefix::ESTIMATED_EFFORT>());

    rnd->round_start_ms = strtoll(start_time_str.c_str(), nullptr, 10);
    rnd->estimated_effort = strtod(estimated_effort_str.c_str(), nullptr);
    rnd->total_effort = strtod(total_effort_str.c_str(), nullptr);
}

void RedisManager::AppendAddRoundShares(std::string_view chain,
                                        const BlockSubmission *submission,
                                        const round_shares_t &miner_shares)
{
    using namespace std::string_view_literals;

    uint32_t height = submission->height;

    for (const auto &[addr, round_share] : miner_shares)
    {
        AppendCommand(
            {"HSET"sv,
             fmt::format("{}:{}", PrefixKey<IMMATURE, REWARD>(), submission->number), addr,
             std::string_view((char *)&round_share, sizeof(RoundShare))});

        AppendCommand({"HINCRBY"sv, fmt::format("{}:{}", PrefixKey<SOLVER>(), addr),
                       EnumName<Prefix::IMMATURE_BALANCE>(),
                       std::to_string(round_share.reward)});
    }
}

bool RedisManager::CloseRound(std::string_view chain, std::string_view type,
                              const ExtendedSubmission *submission,
                              round_shares_t &round_shares, int64_t time_ms)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);
    {
        // either close everything about the round or nothing
        RedisTransaction close_round_tx(this);

        AppendCommand({"INCR"sv, PrefixKey<BLOCK, NUMBER>()});
        AppendAddBlockSubmission(submission);
        AppendAddRoundShares(chain, submission, round_shares);
        // set round start time
        AppendSetMinerEffort(chain, PrefixKey<Prefix::ROUND_START_TIME>(), type, time_ms);
        // reset round total effort
        AppendSetMinerEffort(chain, PrefixKey<Prefix::TOTAL_EFFORT>(), type, 0);

        // reset miners efforts
        for (auto &[addr, _] : round_shares)
        {
            AppendSetMinerEffort(chain, addr, type, 0);
        }
    }

    return GetReplies();
}

int RedisManager::GetBlockNumber()
{
    auto reply = Command({"GET", PrefixKey<BLOCK, NUMBER>()});
    if (reply->type != REDIS_REPLY_STRING) return 0;

    return std::strtol(reply->str, nullptr, 10);
}