#ifndef ROUND_MANAGER_HPP
#define ROUND_MANAGER_HPP

#include <mutex>
#include <atomic>

#include "redis_round.hpp"
#include "redis_manager.hpp"
#include "payment_manager.hpp"
#include "round.hpp"
#include "stats_manager.hpp"
#include "round_share.hpp"

class StatsManager;
class RedisManager;

class RoundManager: public RedisRound
{
   public:
    RoundManager(const RedisManager& rm, const std::string& round_type);
    bool LoadCurrentRound();
    void AddRoundShare(const MinerIdHex& miner, const double effort);
    bool CloseRound(const BlockSubmission& submission, const double fee);
    void ResetRoundEfforts();

    bool UpdateEffortStats(int64_t update_time_ms);
    inline Round GetChainRound() const { return round; };

    void PushPendingShares() {
        std::scoped_lock round_lock(efforts_map_mutex);
        
        this->AddPendingShares(pending_shares);
        pending_shares.clear();
    }

    std::atomic<uint32_t> blocks_found;
    std::atomic<double> netwrok_hr;
    std::atomic<double> difficulty;

   private:
    bool LoadEfforts();

    static constexpr std::string_view field_str = "RoundManager";
    const Logger<field_str> logger;
    const std::string round_type;

#if PAYMENT_SCHEME == PAYMENT_SCHEME_PPLNS
    double round_progress = 0.0;  // PPLNS
    // use raw bytes, to pass directly to redis in one command
    std::vector<Share> pending_shares;
#endif

    std::mutex round_map_mutex;
    Round round;

    std::mutex efforts_map_mutex;
    efforts_map_t efforts_map;
};

#endif