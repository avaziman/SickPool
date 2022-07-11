#ifndef ROUND_MANAGER_HPP
#define ROUND_MANAGER_HPP

#include "block_submission.hpp"
#include "payment_manager.hpp"
#include "redis_manager.hpp"
#include "round.hpp"
#include <mutex>
#include "stats_manager.hpp"

class StatsManager;
class RedisManager;

class RoundManager
{
   public:
    RoundManager(RedisManager* rm, StatsManager* sm)
        : redis_manager(rm), stats_manager(sm), rounds_map()
    {
        LoadCurrentRound();
    }
    bool ClosePowRound(const BlockSubmission* submission, double fee);
    bool ClosePosRound(const BlockSubmission* submission, double fee);

    void LoadCurrentRound();
    void AddRoundEffort(const std::string& chain, const double effort);
    Round GetChainRound(const std::string& chain);

   private:
    bool CloseRound(const std::vector<std::pair<std::string, double>>& efforts, bool is_pos,
                    const BlockSubmission* submission, double fee);

    RedisManager* redis_manager;
    StatsManager* stats_manager;

    std::mutex round_map_mutex;
    round_map rounds_map;
};

#endif