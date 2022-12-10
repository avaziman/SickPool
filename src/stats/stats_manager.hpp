#ifndef STATS_MANAGER_HPP_
#define STATS_MANAGER_HPP_

#include <hiredis/hiredis.h>

#include <vector>
#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>

#include "round_manager.hpp"
#include "difficulty/difficulty_manager.hpp"
#include "logger.hpp"
#include "payments/payment_manager.hpp"
#include "redis_stats.hpp"
#include "round.hpp"
#include "shares/share.hpp"
#include "static_config/static_config.hpp"
#include "stats/stats.hpp"
#include "stratum_client.hpp"
#include "persistence_stats.hpp"

// update manager?
class StatsManager
{
   public:
    explicit StatsManager(const PersistenceLayer& redis_manager, DifficultyManager* diff_manager,
                 RoundManager* round_manager, const StatsConfig* cc);

    // Every hashrate_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    template <StaticConf confs>
    void Start(std::stop_token st);

    bool LoadAvgHashrateSums(int64_t hr_time);
    void AddShare(const worker_map::iterator& it, const double diff);
    bool AddWorker(WorkerFullId& worker_full_id, worker_map::iterator& it,
                   const std::string_view address, std::string_view worker_full,
                   std::time_t curtime, std::string_view alias,
                   int64_t min_payout);
    void PopWorker(worker_map::iterator& it);

    // bool AppendPoSBalances(std::string_view chain, int64_t from_ms);

    template <StaticConf confs>
    bool UpdateIntervalStats(int64_t update_time_ms);

    bool UpdateEffortStats(int64_t update_time_ms);

    static int hashrate_interval_seconds;
    static int effort_interval_seconds;
    static int average_hashrate_interval_seconds;
    static int diff_adjust_seconds;
    static double average_interval_ratio;

   private:
    static constexpr std::string_view field_str = "StatsManager";
    Logger<field_str> logger;
    const StatsConfig* conf;
    PersistenceStats persistence_stats;
    DifficultyManager* diff_manager;
    RoundManager* round_manager;

    // workers that have disconnected from the pool, clean them after stats update
    std::mutex to_remove_mutex;
    std::vector<worker_map::iterator> to_remove;

    std::shared_mutex stats_list_smutex;
    // worker -> stats
    worker_map worker_stats_map;
};

#endif