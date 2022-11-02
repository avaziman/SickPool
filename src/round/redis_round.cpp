#include "redis_round.hpp"

using enum Prefix;

void RedisRound::AppendSetMinerEffort(std::string_view chain,
                                      std::string_view miner,
                                      std::string_view type, double effort)
{
    AppendHset(key_names.round_efforts,
               miner, std::to_string(effort));
}

bool RedisRound::SetEffortStats(const efforts_map_t &miner_stats_map,
                                     const double total_effort,
                                     std::unique_lock<std::mutex> stats_mutex)
{
    std::scoped_lock redis_lock(rc_mutex);

    for (const auto &[miner_addr, miner_effort] : miner_stats_map)
    {
        AppendSetMinerEffort(COIN_SYMBOL, miner_addr, EnumName<POW>(),
                             miner_effort);
    }

    AppendSetMinerEffort(COIN_SYMBOL, EnumName<TOTAL_EFFORT>(), EnumName<POW>(),
                         total_effort);
    stats_mutex.unlock();

    return GetReplies();
}

bool RedisRound::GetMinerEfforts(efforts_map_t &efforts,
                                  std::string_view chain, std::string_view type)
{
    using namespace std::string_view_literals;
    using enum Prefix;

    auto reply = Command({"HGETALL"sv, Format({key_names.round_efforts, chain})});

    for (int i = 0; i < reply->elements; i += 2)
    {
        std::string addr(reply->element[i]->str, reply->element[i]->len);

        double effort = std::strtod(reply->element[i + 1]->str, nullptr);

        efforts[addr] = effort;
    }
    return true;
}

void RedisRound::GetCurrentRound(Round *rnd, std::string_view chain,
                                   std::string_view type)
{
    std::string round_effort_key =
        Format({chain, type, EnumName<ROUND_EFFORT>()});

    std::string total_effort_str =
        hget(round_effort_key, EnumName<TOTAL_EFFORT>());

    std::string start_time_str = hget(round_effort_key, EnumName<START_TIME>());

    std::string estimated_effort_str =
        hget(round_effort_key, EnumName<Prefix::ESTIMATED_EFFORT>());

    rnd->round_start_ms = strtoll(start_time_str.c_str(), nullptr, 10);
    rnd->estimated_effort = strtod(estimated_effort_str.c_str(), nullptr);
    rnd->total_effort = strtod(total_effort_str.c_str(), nullptr);
}

void RedisRound::AppendAddRoundShares(std::string_view chain,
                                        const BlockSubmission *submission,
                                        const round_shares_t &miner_shares)
{
    using namespace std::string_view_literals;

    for (const auto &[addr, round_share] : miner_shares)
    {
        AppendCommand(
            {"HSET"sv,
             Format({key_names.reward_immature,
                         std::to_string(submission->number)}),
             addr,
             std::string_view(reinterpret_cast<const char *>(&round_share),
                              sizeof(RoundShare))});

        AppendCommand({"HINCRBY"sv, Format({key_names.solver, addr}),
                       EnumName<Prefix::IMMATURE_BALANCE>(),
                       std::to_string(round_share.reward)});
    }
}

bool RedisRound::SetClosedRound(std::string_view chain, std::string_view type,
                              const ExtendedSubmission *submission,
                              const round_shares_t &round_shares,
                              int64_t time_ms)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);
    {
        // either close everything about the round or nothing
        RedisTransaction close_round_tx(this);

        AppendCommand({"INCR"sv, key_names.block_number});
        AppendAddBlockSubmission(submission);
        AppendAddRoundShares(chain, submission, round_shares);
        // set round start time
        AppendSetMinerEffort(chain, EnumName<START_TIME>(), type,
                             static_cast<double>(time_ms));
        // reset round total effort
        AppendSetMinerEffort(chain, EnumName<TOTAL_EFFORT>(), type, 0);

        // reset miners efforts
        for (auto &[addr, _] : round_shares)
        {
            AppendSetMinerEffort(chain, addr, type, 0);
        }
    }

    return GetReplies();
}