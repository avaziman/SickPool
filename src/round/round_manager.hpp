#ifndef ROUND_MANAGER_HPP
#define ROUND_MANAGER_HPP

#include <mutex>
#include <atomic>

#include "block_submission.hpp"
#include "payment_manager.hpp"
#include "round.hpp"
#include "redis_manager.hpp"
#include "stats_manager.hpp"
#include "round_share.hpp"

class StatsManager;
class RedisManager;

class RoundManager
{
   public:
    RoundManager(RedisManager* rm, const std::string& round_type);
    bool LoadCurrentRound();
    void AddRoundShare(const std::string& miner, const double effort);
    Round GetChainRound();
    bool CloseRound(const ExtendedSubmission* submission, double fee);
    void ResetRoundEfforts();

    bool LoadUnpaidRewards(std::vector<std::pair<std::string, PayeeInfo>>& rewards);

    bool IsMinerIn(const std::string& addr);

    bool UpdateEffortStats(int64_t update_time_ms);

    std::atomic<uint32_t> blocks_found;
    std::atomic<double> netwrok_hr;
    std::atomic<double> difficulty;

   private:
    bool LoadEfforts();

    RedisManager* redis_manager;
    const std::string round_type;

    std::mutex round_map_mutex;
    Round round;

    std::mutex efforts_map_mutex;
    efforts_map_t efforts_map;
};

#endif