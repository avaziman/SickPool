#ifndef PAYMENT_MANAGER_HPP
#define PAYMENT_MANAGER_HPP
#include "logger.hpp"
#include "stats.hpp"
struct RoundShare
{
    std::string address;
    double effort;
    double share;
    double reward;
};

class PaymentManager
{
   public:
    static bool GetRewardsProp(std::vector<RoundShare>& miner_shares,
                               std::string_view chain, int64_t block_reward,
                               miner_map& miners, double total_effort,
                               double fee)
    {
        double substracted_reward =
            static_cast<double>(block_reward) * (1.f - fee);

        if (total_effort == 0)
        {
            Logger::Log(LogType::Critical, LogField::PaymentManager,
                        "Round effort is 0!");
            return false;
        }
        miner_shares.resize(miners.size());

        for (auto& [addr, stats] : miners)
        {
            RoundShare round_share;
            round_share.address = addr;
            round_share.effort =
                stats.round_effort_map[std::string(chain.data(), chain.size())];
            round_share.share = round_share.effort / total_effort;
            round_share.reward = round_share.share * substracted_reward;
            miner_shares.push_back(round_share);

            Logger::Log(
                LogType::Info, LogField::PaymentManager,
                "Miner round share: %.*s, effort: %f, share: %f, reward: "
                "%f, total effort: %f",
                round_share.address.size(), round_share.address.data(),
                round_share.effort, round_share.share, round_share.reward);
        }

        return true;
    }
};
#endif