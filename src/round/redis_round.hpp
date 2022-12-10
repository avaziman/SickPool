#ifndef REDIS_ROUND_HPP_
#define REDIS_ROUND_HPP_
#include <charconv>
#include <mutex>

#include "redis_block.hpp"
#include "redis_manager.hpp"
class PersistenceRound :  public PersistenceBlock
{
   public:
    explicit PersistenceRound(const PersistenceLayer &pl) : PersistenceBlock(pl) {}

    std::pair<std::span<Share>, redis_unique_ptr> GetLastNShares(
        double progress, double n);

    std::pair<std::span<Share>, redis_unique_ptr> GetSharesBetween(
        ssize_t start, ssize_t end);

    void AddPendingShares(const std::vector<Share> &pending_shares);

    void AppendSetMinerEffort(std::string_view chain, std::string_view miner,
                              std::string_view type, double effort);

    void AppendAddRoundRewards(std::string_view chain,
                               const BlockSubmission& submission,
                               const round_shares_t &miner_shares);

    bool SetClosedRound(std::string_view chain, std::string_view type,
                        const BlockSubmission& submission,
                        const round_shares_t &round_shares, int64_t time_ms);
    void GetCurrentRound(Round *rnd, std::string_view chain,
                         std::string_view type);

    bool GetMinerEfforts(efforts_map_t &efforts, std::string_view chain,
                         std::string_view type);

    bool SetEffortStats(const efforts_map_t &miner_stats_map,
                        const double total_effort,
                        std::unique_lock<std::mutex> stats_mutex);
    bool SetNewBlockStats(std::string_view chain, double target_diff);
};

#endif