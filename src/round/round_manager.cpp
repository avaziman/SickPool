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
    auto [res, rep] = GetSharesBetween(-2, -1);

    if (!res.empty())
    {
        round_progress = res.front().progress;
    }
    else
    {
        round_progress = 0;
    }
#endif
}

bool RoundManager::CloseRound(const BlockSubmission& submission,
                              const double fee)
{
    std::scoped_lock round_lock(round_map_mutex, efforts_map_mutex);

    round_shares_t round_shares;
    // PaymentManager::GetRewardsPROP(round_shares, submission.reward,
    //                                efforts_map, round.total_effort, fee);
    const double n = 2;
    auto [shares, resp] = GetLastNShares(round_progress, n);
    if (!PaymentManager::GetRewardsPPLNS(round_shares, shares,
                                         submission.reward, n, fee))
    {
    }
    // if(!shares.empty())

    round.round_start_ms = submission.time_ms;
    round.total_effort = 0;
    blocks_found++;

    ResetRoundEfforts();

    return SetClosedRound(std::to_string(submission.chain), round_type,
                          submission, round_shares, submission.time_ms);
}

void RoundManager::AddRoundShare(const MinerIdHex& miner, const double effort)
{
    std::scoped_lock round_lock(efforts_map_mutex);

    efforts_map[miner] += effort;
    round.total_effort += effort;

#if PAYMENT_SCHEME == PAYMENT_SCHEME_PPLNS
    round_progress += effort;

    if (pending_shares.capacity() == pending_shares.size())
    {
        pending_shares.reserve(pending_shares.size() + 1000);
    }

    Share curshare{.miner_id = miner.id, .progress = round_progress};

    pending_shares.push_back(std::move(curshare));
#endif
}

bool RoundManager::LoadCurrentRound()
{
    if (!LoadEfforts())
    {
        logger.Log<LogType::Critical>("Failed to load current round efforts!");
        return false;
    }

    auto chain = key_names.coin;

    // no need mutex here
    GetCurrentRound(&round, chain, round_type);

    if (round.round_start_ms == 0)
    {
        round.round_start_ms = GetCurrentTimeMs();

        // append commands are not locking but since its before threads are
        // starting its ok set round start time
        AppendSetMinerEffort(chain, EnumName<START_TIME>(), round_type,
                             static_cast<double>(round.round_start_ms));
        // reset round effort if we are starting it now hadn't started
        AppendSetMinerEffort(chain, EnumName<TOTAL_EFFORT>(), round_type, 0);

        if (!GetReplies())
        {
            logger.Log<LogType::Critical>(
                "Failed to reset round effort and start time");
            return false;
        }
    }

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

bool RoundManager::UpdateEffortStats([[maybe_unused]] int64_t update_time_ms)
{
    // (with mutex)
    std::unique_lock round_lock(round_map_mutex);
    const double total_effort = round.total_effort;

    return SetEffortStats(efforts_map, total_effort, std::move(round_lock));
}

bool RoundManager::LoadEfforts()
{
    bool res = GetMinerEfforts(efforts_map, key_names.coin, round_type);

    std::vector<MinerIdHex> to_remove;
    for (auto& [id, effort] : efforts_map)
    {
        // remove special effort keys such as $total and $estimated
        // erase_if...
        if (effort == 0)
        {
            to_remove.emplace_back(id);
        }
        // else
        // {
        logger.Log<LogType::Info>("Loaded {} effort for address {} of {}",
                                  round_type, id.GetHex(), effort);
        // }
    }

    for (const auto& remove : to_remove)
    {
        efforts_map.erase(remove);
    }

    return res;
}