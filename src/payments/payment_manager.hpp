#ifndef PAYMENT_MANAGER_HPP
#define PAYMENT_MANAGER_HPP
#include "logger.hpp"
#include "stats.hpp"
#include "config.hpp"

#pragma pack(push, 1)

struct RoundShare
{
    double effort;
    double share;
    int64_t reward;
};
#pragma pack(pop)

typedef std::vector<std::pair<std::string, RoundShare>> round_shares_t;

class PaymentManager
{
   public:
    static bool GetRewardsProp(
        round_shares_t& miner_shares,
        int64_t block_reward,
        efforts_map_t& miner_efforts,
        double total_effort, double fee)
    {
        int64_t substracted_reward = static_cast<int64_t>(
            static_cast<double>(block_reward) * (1.f - fee));

        if (total_effort == 0)
        {
            Logger::Log(LogType::Critical, LogField::PaymentManager,
                        "Round effort is 0!");
            return false;
        }
        // miner_shares.resize(miners.size());

        for (auto& [addr, effort] : miner_efforts)
        {
            RoundShare round_share;
            round_share.effort = effort;
            round_share.share = round_share.effort / total_effort;
            round_share.reward =
                static_cast<int64_t>(round_share.share * static_cast<double>(substracted_reward));
            miner_shares.emplace_back(addr, round_share);

            Logger::Log(LogType::Info, LogField::PaymentManager,
                        "Miner round share: {}, effort: {}, share: {}, reward: "
                        "{}, total effort: {}",
                        addr, round_share.effort, round_share.share,
                        round_share.reward, total_effort);
        }

        return true;
    }
};
#endif