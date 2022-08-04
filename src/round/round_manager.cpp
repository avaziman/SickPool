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

    Round& round = rounds_map[std::string(submission->chain_sv.data())];

    round_shares_t round_shares;
    PaymentManager::GetRewardsProp(round_shares, submission->block_reward,
                                   efforts_map, round.total_effort, fee);

    round.round_start_ms = submission->time_ms;
    round.total_effort = 0;

    ResetRoundEfforts();

    return redis_manager->CloseRound(submission->chain_sv, round_type,
                                     submission, round_shares,
                                     submission->time_ms);
}

void RoundManager::AddRoundShare(const std::string& chain,
                                 const std::string& miner, const double effort)
{
    std::scoped_lock round_lock(round_map_mutex);

    efforts_map[miner] += effort;
    rounds_map[chain].total_effort += effort;
}

Round RoundManager::GetChainRound(const std::string& chain)
{
    std::scoped_lock round_lock(round_map_mutex);
    return rounds_map[chain];
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

    auto rnd = &rounds_map[chain];
    redis_manager->LoadCurrentRound(chain, round_type, rnd);

    if (rnd->round_start_ms == 0)
    {
        rnd->round_start_ms = GetCurrentTimeMs();

        // append commands are not locking but since its before threads are
        // starting its ok set round start time
        redis_manager->AppendSetMinerEffort(chain,
                                            RedisManager::ROUND_START_TIME_KEY,
                                            round_type, rnd->round_start_ms);
        // reset round effort if we are starting it now hadn't started
        redis_manager->AppendSetMinerEffort(
            chain, RedisManager::TOTAL_EFFORT_KEY, round_type, 0);

        if (!redis_manager->GetReplies())
        {
            Logger::Log(LogType::Critical, LogField::RoundManager,
                        "Failed to reset round effort and start time");
            return false;
        }
    }

    Logger::Log(LogType::Info, LogField::RoundManager,
                "Loaded round chain: {}, effort of: {}, started at: {}", chain,
                rnd->total_effort, rnd->round_start_ms);
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
    const double total_effort = GetChainRound(COIN_SYMBOL).total_effort;

    return redis_manager->UpdateEffortStats(efforts_map, total_effort,
                                            &round_map_mutex);
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
// TODO: make this only for signular chain

bool RoundManager::LoadMatureRewards(
    std::vector<std::pair<std::string, RewardInfo>>& rewards,
    uint32_t block_num)
{
    return redis_manager->LoadMatureRewards(rewards, efforts_map,
                                            &efforts_map_mutex, block_num);
}