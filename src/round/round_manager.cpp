#include "round_manager.hpp"

RoundManager::RoundManager(RedisManager* rm, const std::string& round_type)
    : redis_manager(rm), round_type(round_type)
{
    if (!LoadCurrentRound())
    {
        Logger::Log(LogType::Critical, LogField::RoundManager,
                    "Failed to load current round!");
    }

    if (!redis_manager->ResetMinersWorkerCounts(efforts_map,
                                                GetCurrentTimeMs()))
    {
        Logger::Log(LogType::Critical, LogField::RoundManager,
                    "Failed to reset worker counts!");
    }
}

bool RoundManager::CloseRound(const ExtendedSubmission* submission, double fee)
{
    std::scoped_lock round_lock(round_map_mutex);

    round_shares_t round_shares;
    PaymentManager::GetRewardsProp(round_shares, submission->block_reward,
                                   efforts_map, round.total_effort, fee);

    round.round_start_ms = submission->time_ms;
    round.total_effort = 0;
    blocks_found++;

    ResetRoundEfforts();

    return redis_manager->CloseRound(submission->chain_sv, round_type,
                                     submission, round_shares,
                                     submission->time_ms);
}

void RoundManager::AddRoundShare(
                                 const std::string& miner, const double effort)
{
    std::scoped_lock round_lock(efforts_map_mutex);

    efforts_map[miner] += effort;
    round.total_effort += effort;
}

Round RoundManager::GetChainRound()
{
    return round;
}

bool RoundManager::LoadCurrentRound()
{
    if (!LoadEfforts())
    {
        Logger::Log(LogType::Critical, LogField::StatsManager,
                    "Failed to load current round efforts!");
        return false;
    }

    auto chain = std::string(COIN_SYMBOL);

    // no need mutex here
    redis_manager->LoadCurrentRound(chain, round_type, &round);

    if (round.round_start_ms == 0)
    {
        round.round_start_ms = GetCurrentTimeMs();

        // append commands are not locking but since its before threads are
        // starting its ok set round start time
        redis_manager->AppendSetMinerEffort(chain,
                                            PrefixKey<Prefix::ROUND_START_TIME>(),
                                            round_type, round.round_start_ms);
        // reset round effort if we are starting it now hadn't started
        redis_manager->AppendSetMinerEffort(
            chain, PrefixKey<Prefix::TOTAL_EFFORT>(), round_type, 0);

        if (!redis_manager->GetReplies())
        {
            Logger::Log(LogType::Critical, LogField::RoundManager,
                        "Failed to reset round effort and start time");
            return false;
        }
    }

    Logger::Log(LogType::Info, LogField::RoundManager,
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

bool RoundManager::UpdateEffortStats(int64_t update_time_ms)
{
    // (with mutex)
    std::unique_lock round_lock(round_map_mutex);
    const double total_effort = round.total_effort;

    return redis_manager->UpdateEffortStats(efforts_map, total_effort,
                                            std::move(round_lock));
}

bool RoundManager::LoadEfforts()
{
    bool res =
        redis_manager->LoadMinersEfforts(COIN_SYMBOL, round_type, efforts_map);

    auto it = efforts_map.begin();

    std::vector<std::string> to_remove;
    for (auto& [addr, effort] : efforts_map)
    {
        // remove special effort keys such as $total and $estimated
        if (addr.length() != ADDRESS_LEN)
        {
            to_remove.emplace_back(addr);
        }
        else
        {
            Logger::Log(LogType::Info, LogField::StatsManager,
                        "Loaded {} effort for address {} of {}", round_type,
                        addr, effort);
        }
    }

    for (const auto& remove : to_remove)
    {
        efforts_map.erase(remove);
    }

    return res;
}

bool RoundManager::IsMinerIn(const std::string& addr)
{
    std::scoped_lock lock(efforts_map_mutex);
    return efforts_map.contains(addr);
}
// TODO: make this only for singular chain

bool RoundManager::LoadUnpaidRewards(
    std::vector<std::pair<std::string, PayeeInfo>>& rewards)
{
    return redis_manager->LoadUnpaidRewards(rewards, efforts_map,
                                            &efforts_map_mutex);
}