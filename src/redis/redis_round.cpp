#include "redis_manager.hpp"

void RedisManager::AppendSetMinerEffort(std::string_view chain,
                                        std::string_view miner,
                                        std::string_view type, double effort)
{
    AppendHset(fmt::format("{}:{}:{}", chain, type, ROUND_EFFORT_KEY), miner,
               std::to_string(effort));
}

void RedisManager::LoadCurrentRound(std::string_view chain, std::string_view type, Round *rnd)
{
    std::string round_effort_key =
        fmt::format("{}:{}:{}", chain, type, ROUND_EFFORT_KEY);

    std::string total_effort_str = hget(
        round_effort_key,
        TOTAL_EFFORT_KEY);

    std::string start_time_str = hget(
        round_effort_key,
        ROUND_START_TIME_KEY);

    std::string estimated_effort_str =
        hget(round_effort_key, ESTIMATED_EFFORT_KEY);

    rnd->round_start_ms = strtoll(start_time_str.c_str(), nullptr, 10);
    rnd->estimated_effort = strtod(estimated_effort_str.c_str(), nullptr);
    rnd->total_effort = strtod(total_effort_str.c_str(), nullptr);
}

void RedisManager::AppendAddRoundShares(std::string_view chain,
                                        const BlockSubmission *submission,
                                        const round_shares_t &miner_shares)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    uint32_t height = submission->height;

    for (const auto &[addr, round_share] : miner_shares)
    {
        AppendCommand({"HSET"sv,
                       fmt::format("immature-rewards:{}", submission->number),
                       std::to_string(submission->number), addr,
                       std::string_view((char *)&round_share,
                                        sizeof(RoundShare))});

        AppendCommand({
            "HINCRYBY"sv,
            fmt::format("solver:{}", addr),
            std::to_string(round_share.reward)
        });
    }
}

bool RedisManager::CloseRound(std::string_view chain, std::string_view type,
                              const ExtendedSubmission *submission,
                              round_shares_t round_shares, int64_t time_ms)
{
    using namespace std::string_view_literals;

    {
        // either close everything about the round or nothing
        RedisTransaction close_round_tx(this);

        AppendCommand({"INCR"sv, BLOCK_NUMBER_KEY});
        AppendAddBlockSubmission(submission);
        AppendAddRoundShares(chain, submission, round_shares);
        // set round start time
        AppendSetMinerEffort(chain, ROUND_START_TIME_KEY, type, time_ms);
        // reset round total effort
        AppendSetMinerEffort(chain, TOTAL_EFFORT_KEY, type, 0);

        // reset miners efforts
        for (auto &[addr, _] : round_shares)
        {
            AppendSetMinerEffort(chain, addr, type, 0);
        }
    }

    return GetReplies();
}