#include "round_manager.hpp"

using enum Prefix;

RoundManager::RoundManager(const PersistenceLayer& pl,
                           const std::string& round_type)
    : PersistenceRound(pl), round_type(round_type)
{
    if (!LoadCurrentRound())
    {
        logger.Log<LogType::Critical>("Failed to load current round!");
    }

#if PAYMENT_SCHEME == PAYMENT_SCHEME_PPLNS
    // get the last share
    // auto [res, rep] = GetSharesBetween(-2, -1);


#endif
}

RoundCloseRes RoundManager::CloseRound(uint32_t& block_id,
                                       const BlockSubmission& submission,
                                       const double fee)
{
    PushPendingShares();  // push all pending shares, before locking

    std::scoped_lock round_lock(round_map_mutex, efforts_map_mutex);

    round_shares_t round_shares;

    const double n = 2;

    // auto [shares, resp] = GetLastNShares(round_progress, n);
    // if (!PayoutManager::GetRewardsPPLNS(round_shares, shares,
    //                                      submission.reward, n, fee))
    // {
    //     return RoundCloseRes::BAD_SHARES_SUM;
    // }

    // round.round_start_ms = submission.time_ms;
    // round.total_effort = 0;

    ResetRoundEfforts();

    return SetClosedRound(block_id, submission, round_shares,
                          submission.time_ms);
}

void RoundManager::AddRoundShare(const MinerId miner_id, const double effort)
{
    std::scoped_lock round_lock(efforts_map_mutex);

    efforts_map[miner_id] += effort;
    round.total_effort += effort;
}

void RoundManager::AddRoundSharePPLNS(const MinerId miner_id,
                                      const double effort)
{
    if (pending_shares.capacity() == pending_shares.size())
    {
        pending_shares.reserve(pending_shares.size() + 1000);
    }

    pending_shares.emplace_back(
        Share{.miner_id = miner_id, .diff = effort});
}

bool RoundManager::LoadCurrentRound()
{
    // if (!LoadEfforts())
    // {
    //     logger.Log<LogType::Critical>("Failed to load current round efforts!");
    //     return false;
    // }

    auto chain = key_names.coin;

    // no need mutex here
    GetCurrentRound(&round, chain, round_type);

    logger.Log<LogType::Info>(
        "Loaded round chain: {}, effort of: {}, started at: {}", chain,
        round.total_effort, round.round_start_ms);
    return true;
}

// already mutexed when called
void RoundManager::ResetRoundEfforts()
{
    for (auto& [miner, effort] : efforts_map)
    {
        effort = 0;
    }
}