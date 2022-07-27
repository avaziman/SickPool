#include "round_manager.hpp"

// bool RoundManager::ClosePowRound(const BlockSubmission* submission, double
// fee)
// {
//     // std::vector<std::pair<std::string, double>> efforts;
//     // stats_manager->GetMiningEffortsReset(
//     //     efforts,
//     //     std::string((char*)submission->chain, sizeof(submission->chain)));

//     // return CloseRound(efforts, false, submission, fee);
//     return false;
// }
// bool RoundManager::ClosePosRound(const BlockSubmission* submission, double
// fee)
// {
//     std::vector<std::pair<std::string, double>> efforts;
//     redis_manager->GetPosPoints(
//         efforts,
//         std::string((char*)submission->chain, sizeof(submission->chain)));

//     return CloseRound(efforts, true, submission, fee);
// }

RoundManager::RoundManager(RedisManager* rm, const std::string& round_type)
    : redis_manager(rm), round_type(round_type)
{
    if (!LoadCurrentRound())
    {
        Logger::Log(LogType::Critical, LogField::RoundManager,
                    "Failed to load current round!");
    }

    if (!redis_manager->ResetMinersWorkerCounts(efforts_map, GetCurrentTimeMs()))
    {
        Logger::Log(LogType::Critical, LogField::RoundManager,
                    "Failed to reset worker counts!");
    }

}

bool RoundManager::CloseRound(
    const BlockSubmission* submission, double fee)
{
    std::scoped_lock round_lock(round_map_mutex);

    std::string chain =
        std::string((char*)submission->chain, sizeof(submission->chain));
    Round& round = rounds_map[chain];

    round_shares_t round_shares;
    PaymentManager::GetRewardsProp(round_shares, submission->block_reward,
                                   efforts_map, round.total_effort, fee);

    round.round_start_ms = submission->time_ms;
    round.total_effort = 0;

    ResetRoundEfforts(chain);

    return redis_manager->CloseRound(chain, round_type, submission, round_shares,
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
    double total_effort = redis_manager->GetRoundEffort(chain, "pow");
    int64_t round_start = redis_manager->GetRoundTime(chain, "pow");

    if (round_start == 0)
    {
        round_start = GetCurrentTimeMs();

        redis_manager->SetRoundStartTime(chain, "pow", round_start);
        // reset round effort if we are starting it now hadn't started
        redis_manager->SetMinerEffort(chain, TOTAL_EFFORT_KEY, "pow", 0);
    }

    auto round = &rounds_map[chain];
    round->round_start_ms = round_start;
    round->total_effort = total_effort;

    Logger::Log(LogType::Info, LogField::RoundManager,
                "Loaded round chain: {}, effort of: {}, started at: {}",
                chain, round->total_effort, round->round_start_ms);
    return true;
}

// already mutexed when called
void RoundManager::ResetRoundEfforts(const std::string& chain)
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
    bool res = redis_manager->LoadMinersEfforts(COIN_SYMBOL, round_type, efforts_map);

    auto it = efforts_map.begin();

    std::vector<std::string> to_remove;
    for (auto& [addr, effort] : efforts_map)
    {
        // remove special effort keys such as $total and $estimated
        if(addr.length() != ADDRESS_LEN){
            to_remove.emplace_back(addr);
        }
        else
        {
            Logger::Log(LogType::Info, LogField::StatsManager,
                        "Loaded {} effort for address {} of {}", round_type, addr, effort);
        }
    }

    for (const auto& remove : to_remove){
        efforts_map.erase(remove);
    }

    return res;
}

bool RoundManager::IsMinerIn(const std::string& addr){
    std::scoped_lock lock(efforts_map_mutex);
    return efforts_map.contains(addr);
}
//TODO: make this only for signular chain 