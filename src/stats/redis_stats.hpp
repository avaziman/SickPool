#ifndef REDIS_STATS_HPP_
#define REDIS_STATS_HPP_

#include <charconv>

#include "redis_manager.hpp"

class RedisStats : public RedisManager
{
   public:
    explicit RedisStats(const RedisManager &rm) : RedisManager(rm)
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

    bool AddNewMiner(std::string_view address, std::string_view addr_lowercase,
                     std::string_view alias, const MinerIdHex &id,
                     int64_t curtime, int64_t min_payout);

    bool AddNewWorker(const WorkerFullId &full_id,
                      std::string_view address_lowercase,
                      std::string_view worker_name, std::string_view alias);

    void AppendIntervalStatsUpdate(std::string_view addr,
                                   std::string_view prefix,
                                   int64_t update_time_ms,
                                   const WorkerStats &ws);

    bool GetMinerId(MinerIdHex &id, std::string_view addr_lc);
    bool GetWorkerId(WorkerIdHex &worker_id, const MinerIdHex &miner_id,
                     std::string_view worker_name);

    bool UpdateIntervalStats(worker_map &worker_stats_map,
                             miner_map &miner_stats_map,
                             std::mutex *stats_mutex, double net_hr,
                             double diff, uint32_t blocks_found,
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
    void AppendCreateStatsTs(std::string_view addrOrWorker, std::string_view id,
                             std::string_view prefix,
                             std::string_view addr_lowercase_sv);

    bool PopWorker(const WorkerFullId& fullid);
};

#endif