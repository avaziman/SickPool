#ifndef STATS_MANAGER_HPP_
#define STATS_MANAGER_HPP_

#include <hiredis/hiredis.h>

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "difficulty/difficulty_manager.hpp"
#include "logger.hpp"
#include "payout_manager.hpp"
#include "config_zano.hpp"
#include "persistence_stats.hpp"
#include "redis_stats.hpp"
#include "round.hpp"
#include "round_manager.hpp"
#include "shares/share.hpp"
#include "static_config/static_config.hpp"
#include "stats/stats.hpp"
#include "stratum_client.hpp"

// update manager?
class StatsManager
{
   public:
    explicit StatsManager(const PersistenceLayer& redis_manager,
                          RoundManager* round_manager, const StatsConfig* cc, double hash_multiplier);

    // Every hashrate_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    void Start(std::stop_token st);

    bool LoadAvgHashrateSums(int64_t hr_time);
    void AddValidShare(const worker_map::iterator& it, const double diff);
    void AddInvalidShare(const worker_map::iterator& it);
    void AddStaleShare(const worker_map::iterator& it);
    bool AddWorker(int64_t& worker_id,
                   int64_t miner_id, std::string_view address,
                   std::string_view worker_name, std::string_view alias);

    bool AddMiner(int64_t& miner_id, std::string_view address,
                  std::string_view alias, int64_t min_payout);

    void PopWorker(const worker_map::iterator& it);

    bool UpdateIntervalStats(int64_t update_time_ms);

    bool UpdateEffortStats(int64_t update_time_ms);
    void SetNetworkStats(const NetworkStats& ns) { network_stats = ns; }
    worker_map::iterator AddExistingWorker(FullId workerid);
    static uint32_t average_interval_ratio;

   private:
    static constexpr std::string_view field_str = "StatsManager";
    Logger logger{field_str};
    const StatsConfig* conf;
    PersistenceStats persistence_stats;
    RoundManager* round_manager;
    const double hash_multiplier;

    NetworkStats network_stats;
    // workers that have disconnected from the pool, clean them after stats
    // update
    std::mutex to_remove_mutex;
    // it + amount of iterations, remove only when average is zero
    std::vector<std::pair<worker_map::iterator, uint32_t>> to_remove;

    std::shared_mutex stats_list_smutex;
    // worker -> stats
    worker_map worker_stats_map;
};

#endif