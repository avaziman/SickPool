#ifndef ROUND_MANAGER_HPP
#define ROUND_MANAGER_HPP

#include <mutex>

#include "block_submission.hpp"
#include "payment_manager.hpp"
#include "round.hpp"
#include "stats_manager.hpp"

class StatsManager;
class RedisManager;

class RoundManager
{
   public:
    RoundManager(RedisManager* rm, bool is_pos)
        : redis_manager(rm), is_pos(is_pos)
    {
        LoadCurrentRound();
    }
    void LoadCurrentRound();
    void AddRoundShare(const std::string& chain, const std::string& miner, const double effort);
    Round GetChainRound(const std::string& chain);
    bool CloseRound(const std::vector<std::pair<std::string, double>>& efforts, const BlockSubmission* submission, double fee);
    void ResetRoundEfforts(const std::string& chain);

    bool UpdateEffortStats(int64_t update_time_ms);

   private:
    bool LoadEfforts();
    
    RedisManager* redis_manager;
    const bool is_pos;

    std::mutex round_map_mutex;
    round_map_t rounds_map;

    std::mutex efforts_map_mutex;
    efforts_map_t efforts_map;
};

#endif