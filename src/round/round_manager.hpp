#ifndef ROUND_MANAGER_HPP
#define ROUND_MANAGER_HPP

#include <mutex>
#include <atomic>

#include "redis_round.hpp"
#include "redis_manager.hpp"
#include "block_submission.hpp"
#include "payment_manager.hpp"
#include "round.hpp"
#include "stats_manager.hpp"
#include "round_share.hpp"

class StatsManager;
class RedisManager;

class RoundManager : public RedisRound
{
   public:
    RoundManager(const RedisManager& rm, const std::string& round_type);
    bool LoadCurrentRound();
    void AddRoundShare(const std::string& miner, const double effort);
    bool CloseRound(const ExtendedSubmission* submission, double fee);
    void ResetRoundEfforts();

    bool IsMinerIn(const std::string& addr);

    bool UpdateEffortStats(int64_t update_time_ms);
    inline Round GetChainRound() const { return round; };

    std::atomic<uint32_t> blocks_found;
    std::atomic<double> netwrok_hr;
    std::atomic<double> difficulty;

   private:
    bool LoadEfforts();

    static constexpr std::string_view field_str = "RoundManager";
    const Logger<field_str> logger;
    const std::string round_type;

    std::mutex round_map_mutex;
    Round round;

    std::mutex efforts_map_mutex;
    efforts_map_t efforts_map;
};

#endif