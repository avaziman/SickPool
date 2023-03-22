#ifndef REDIS_STATS_HPP_
#define REDIS_STATS_HPP_

#include <charconv>
#include <shared_mutex>

#include "redis_manager.hpp"

class RedisStats : public RedisManager
{
   public:
    explicit RedisStats(const RedisManager &rm)
        : RedisManager(rm),
          average_hashrate_ratio_str(std::to_string(
              this->conf->stats.average_hashrate_interval_seconds /
              this->conf->stats.hashrate_interval_seconds))
    {
        // std::vector<MinerIdHex> active_ids;
        // if (!GetActiveIds(active_ids))
        // {
        //     logger.Log<LogType::Critical>("Failed to get active ids!");
        // }

        // if (!ResetMinersWorkerCounts(active_ids, GetCurrentTimeMs()))
        // {
        //     logger.Log<LogType::Critical>("Failed to reset worker counts!");
        // }
    }

    const std::string average_hashrate_ratio_str;

    bool CreateMinerStats(std::string_view addr_lowercase,
                          std::optional<std::string_view> alias, MinerId id,
                          int64_t curtime);

    bool CreateWorkerStats(FullId full_id, std::string_view address_lowercase,
                           std::string_view worker_name,
                           std::optional<std::string_view> alias,
                           uint64_t curtime_ms);

    void AppendIntervalStatsUpdate(sw::redis::Pipeline &pipe,
                                   std::string_view addr,
                                   std::string_view prefix,
                                   int64_t update_time_ms,
                                   const WorkerStats &ws);

    bool UpdateIntervalStats(worker_map &worker_stats_map,
                             miner_map &miner_stats_map,
                             std::unique_lock<std::shared_mutex> stats_mutex,
                             const NetworkStats &ns, int64_t update_time_ms);

    void AppendUpdateWorkerCount(sw::redis::Pipeline &pipe, MinerId miner_id,
                                 int amount, int64_t update_time_ms);
    void AppendCreateStatsTsMiner(sw::redis::Pipeline &pipe,
                                  std::string_view addr,
                                  std::optional<std::string_view> id,
                                  std::string_view addr_lowercase_sv,
                                  uint64_t curtime_ms);
    void AppendCreateStatsTsWorker(sw::redis::Pipeline &pipe,
                                   std::string_view addr,
                                   std::optional<std::string_view> id,
                                   std::string_view addr_lowercase_sv,
                                   std::string_view worker_name,
                                   uint64_t curtime_ms);

    bool PopWorker(WorkerId fullid);
};

#endif