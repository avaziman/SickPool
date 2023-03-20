#include "payout_manager.hpp"

Logger PayoutManager::logger{PayoutManager::field_str};

inline void AddSharePPLNS(round_shares_t& miner_shares, double& score_sum,
                          const double n, const double score,
                          const int64_t block_reward, const double fee,
                          const MinerId id)
{
    const double share_ratio = score / n;
    const auto reward = static_cast<int64_t>(share_ratio * (1.0 - fee) *
                                             static_cast<double>(block_reward));

    score_sum += score;

    miner_shares[id].reward += reward;
    miner_shares[id].share += share_ratio;
    miner_shares[id].effort += 1;
}

bool PayoutManager::GetRewardsPPLNS(round_shares_t& miner_shares,
                                    const std::span<Share> shares,
                                    const int64_t block_reward, const double n,
                                    const double fee)
{
    double score_sum = 0;
    miner_shares.reserve(shares.size());

    if (shares.empty()) return false;

    for (ssize_t i = shares.size() - 1; i > 0; i--)
    {
        const double score = shares[i].diff;

        AddSharePPLNS(miner_shares, score_sum, n, score, block_reward, fee,
                      shares[i].miner_id);
    }

    if (score_sum >= n)
    {
        return false;
    }

    const double last_score = n - score_sum;

    AddSharePPLNS(miner_shares, score_sum, n, last_score, block_reward, fee,
                  shares[0].miner_id);

    return true;
}

bool PayoutManager::GetRewardsPROP(round_shares_t& miner_shares,
                                   const int64_t block_reward,
                                   const efforts_map_t& miner_efforts,
                                   double total_effort, double fee)
{
    auto substracted_reward =
        static_cast<int64_t>(static_cast<double>(block_reward) * (1.f - fee));

    if (total_effort == 0)
    {
        logger.template Log<LogType::Critical>("Round effort is 0!");
        return false;
    }
    miner_shares.reserve(miner_efforts.size());

    for (auto& [miner_id, effort] : miner_efforts)
    {
        RoundReward round_share;
        round_share.effort = effort;
        round_share.share = round_share.effort / total_effort;
        round_share.reward = static_cast<int64_t>(
            round_share.share * static_cast<double>(substracted_reward));
        miner_shares.try_emplace(miner_id, round_share);

        logger.template Log<LogType::Info>(
            "Miner round share: {}, effort: {}, share: {}, reward: "
            "{}, total effort: {}",
            miner_id, round_share.effort, round_share.share, round_share.reward,
            total_effort);
    }

    return true;
}