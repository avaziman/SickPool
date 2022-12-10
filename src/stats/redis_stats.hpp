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
          average_hashrate_ratio_str(
              std::to_string(this->conf->stats.average_hashrate_interval_seconds /
                             this->conf->stats.hashrate_interval_seconds))
    {
        std::vector<MinerIdHex> active_ids;
        if (!GetActiveIds(active_ids))
        {
            logger.Log<LogType::Critical>("Failed to get active ids!");
        }

        if (!ResetMinersWorkerCounts(active_ids, GetCurrentTimeMs()))
        {
            logger.Log<LogType::Critical>("Failed to reset worker counts!");
        }
    }

    const std::string average_hashrate_ratio_str;

    bool AddNewMiner(std::string_view address, std::string_view addr_lowercase,
                     std::string_view alias, const MinerIdHex &id,
                     int64_t curtime, int64_t min_payout);

    bool AddNewWorker(const WorkerFullId &full_id,
                      std::string_view address_lowercase,
                      std::string_view worker_name, std::string_view alias,
                      uint64_t curtime_ms);

    void AppendIntervalStatsUpdate(std::string_view addr,
                                   std::string_view prefix,
                                   int64_t update_time_ms,
                                   const WorkerStats &ws);

    bool UpdateIntervalStats(worker_map &worker_stats_list,
                             miner_map &miner_stats_map,
                             std::unique_lock<std::shared_mutex> stats_mutex,
                             double net_hr, double diff, uint32_t blocks_found,
                             int64_t update_time_ms);

    // bool SetNewBlockStats(std::string_view chain, int64_t curtime,
    //                       double net_hr, double estimated_shares);
    bool ResetMinersWorkerCounts(const std::vector<MinerIdHex> &addresses,
                                 int64_t time_now);
    bool LoadAverageHashrateSum(
        std::vector<std::pair<WorkerFullId, double>> &hashrate_sums,
        std::string_view prefix, int64_t hr_time, int64_t period);

    void AppendUpdateWorkerCount(MinerIdHex miner_id, int amount,
                                 int64_t update_time_ms);
    void AppendCreateStatsTsMiner(std::string_view addr, std::string_view id,
                             std::string_view addr_lowercase_sv,
                             uint64_t curtime_ms);
    void AppendCreateStatsTsWorker(std::string_view addr, std::string_view id,
                             std::string_view addr_lowercase_sv, std::string_view worker_name,
                             uint64_t curtime_ms);

    bool PopWorker(const WorkerFullId &fullid);
    long GetMinerCount()
    {
        return GetInt(key_names.solver_count);
    }
    long GetWorkerCount(const MinerIdHex& id)
    {
        return GetInt(Format({key_names.miner_worker_count, id.GetHex()}));
    }
};

#endif