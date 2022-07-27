#ifndef ROUND_MANAGER_HPP
#define ROUND_MANAGER_HPP

#include <mutex>

#include "block_submission.hpp"
#include "payment_manager.hpp"
#include "round.hpp"
#include "redis_manager.hpp"
#include "stats_manager.hpp"

class StatsManager;
class RedisManager;

class RoundManager
{
   public:
    RoundManager(RedisManager* rm, const std::string& round_type);
    bool LoadCurrentRound();
    void AddRoundShare(const std::string& chain, const std::string& miner, const double effort);
    Round GetChainRound(const std::string& chain);
    bool CloseRound(const BlockSubmission* submission, double fee);
    void ResetRoundEfforts(const std::string& chain);

    bool IsMinerIn(const std::string& addr);

    bool UpdateEffortStats(int64_t update_time_ms);


   private:
    bool LoadEfforts();

    RedisManager* redis_manager;
    const std::string round_type;

    std::mutex round_map_mutex;
    round_map_t rounds_map;

    std::mutex efforts_map_mutex;
    efforts_map_t efforts_map;
};

#endif