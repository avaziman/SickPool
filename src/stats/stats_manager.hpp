#ifndef STATS_MANAGER_HPP_
#define STATS_MANAGER_HPP_

#include <hiredis/hiredis.h>

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "difficulty/difficulty_manager.hpp"
#include "blocks/block_submission.hpp"
#include "logger.hpp"
#include "payments/payment_manager.hpp"
#include "redis/redis_manager.hpp"
#include "round.hpp"
#include "shares/share.hpp"
#include "static_config/config.hpp"
#include "stats/stats.hpp"

#define UPDATE_EFFORT 0b1
#define UPDATE_INTERVAL 0b01
#define UPDATE_DIFFICULTY 0b001

class RedisManager;
// update manager?
class StatsManager
{
   public:
    StatsManager(RedisManager* redis_manager, DifficultyManager* diff_manager, int hr_interval,
                 int effort_interval, int avg_hr_interval,
                 int diff_adjust_seconds, int hashrate_ttl);

    // Every hashrate_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    // ) staker points
    void Start();
    bool LoadCurrentRound();
    void AddShare(const std::string& worker_full, const std::string& miner_addr,
                  const double diff);
    bool AddWorker(const std::string& address, const std::string& worker_full,
                   const std::string& idTag, std::time_t curtime);
    void PopWorker(const std::string& worker, const std::string& address);

    bool ClosePoWRound(const std::string& chain,
                       const BlockSubmission* submission, double fee);

    // bool AppendPoSBalances(std::string_view chain, int64_t from_ms);

    bool UpdateStats(int64_t update_time_ms, uint8_t update_flags);
    Round GetChainRound(const std::string& chain);

    static int hashrate_interval_seconds;
    static int effort_interval_seconds;
    static int average_hashrate_interval_seconds;
    static int diff_adjust_seconds;
    static int hashrate_ttl_seconds;

   private:
    RedisManager* redis_manager;
    DifficultyManager* diff_manager;

    std::mutex stats_map_mutex;
    // worker -> stats
    worker_map worker_stats_map;
    // miner -> stats
    miner_map miner_stats_map;
    // chain -> effort
    round_map round_stats_map;
};

#endif
// TODO: on crash load all the stats from redis (round hash)
// TODO: add another unordered map for stakers (non miners too!)