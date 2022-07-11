#include "round_manager.hpp"

bool RoundManager::ClosePowRound(const BlockSubmission* submission, double fee)
{
    std::vector<std::pair<std::string, double>> efforts;
    stats_manager->GetMiningEffortsReset(
        efforts,
        std::string((char*)submission->chain, sizeof(submission->chain)));

    return CloseRound(efforts, false, submission, fee);
}
bool RoundManager::ClosePosRound(const BlockSubmission* submission, double fee)
{
    std::vector<std::pair<std::string, double>> efforts;
    redis_manager->GetPosPoints(
        efforts,
        std::string((char*)submission->chain, sizeof(submission->chain)));

    return CloseRound(efforts, true, submission, fee);
}

bool RoundManager::CloseRound(
    const std::vector<std::pair<std::string, double>>& efforts, bool is_pos,
    const BlockSubmission* submission, double fee)
{
    std::scoped_lock round_lock(round_map_mutex);

    std::string chain =
        std::string((char*)submission->chain, sizeof(submission->chain));
    Round& round = rounds_map[chain];

    std::vector<std::pair<std::string, RoundShare>> round_shares;
    PaymentManager::GetRewardsProp(round_shares, submission->block_reward,
                                   efforts, round.total_effort, fee);

    round.round_start_ms = submission->time_ms;
    round.total_effort = 0;

    std::string type = is_pos ? "pos" : "pow";

    if (!redis_manager->AddBlockSubmission(submission))
    {
        Logger::Log(LogType::Critical, LogField::RoundManager,
                    "Failed to add block submission.");
        return false;
    }
    // reset effort of miners + total round effort
    if (!redis_manager->ResetRoundEfforts(chain, type))
    {
        Logger::Log(LogType::Critical, LogField::RoundManager,
                    "Failed to reset round efforts.");
        return false;
    }

    if (!redis_manager->AddRoundShares(chain, submission, round_shares))
    {
        Logger::Log(LogType::Critical, LogField::RoundManager,
                    "Failed to add round shares.");
        return false;
    }

    if (!redis_manager->SetRoundStartTime(chain, type, submission->time_ms))
    {
        Logger::Log(LogType::Critical, LogField::RoundManager,
                    "Failed to set new round time.");
        return false;
    }

    return true;
}

void RoundManager::AddRoundEffort(const std::string& chain, double effort)
{
    std::scoped_lock round_lock(round_map_mutex);

    rounds_map[chain].total_effort += effort;
}

Round RoundManager::GetChainRound(const std::string& chain)
{
    std::scoped_lock round_lock(round_map_mutex);
    return rounds_map[chain];
}

void RoundManager::LoadCurrentRound()
{
    auto chain = std::string(COIN_SYMBOL);
    double total_effort = redis_manager->GetRoundEffort(chain, "pow");
    int64_t round_start = redis_manager->GetRoundTime(chain, "pow");

    if (round_start == 0)
    {
        round_start = GetCurrentTimeMs();

        redis_manager->SetRoundStartTime(chain, "pow", round_start);
        redis_manager->SetRoundEffort(chain, "pow", total_effort);
    }

    auto round = &rounds_map[chain];
    round->round_start_ms = round_start;
    round->total_effort = total_effort;

    Logger::Log(LogType::Info, LogField::StatsManager,
                "Loaded pow round chain: {}, effort of: {}, started at: {}",
                chain, round->total_effort, round->round_start_ms);
}