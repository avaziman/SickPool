#ifndef ROUND_MANAGER_HPP
#define ROUND_MANAGER_HPP

#include <atomic>
#include <mutex>

#include "payout_manager.hpp"
#include "redis_manager.hpp"
#include "redis_round.hpp"
#include "round.hpp"
#include "round_share.hpp"

class RedisManager;

struct RoundStats
{
    uint32_t blocks_found;
};

class RoundManager : public PersistenceRound
{
   public:
    explicit RoundManager(const PersistenceLayer& pl,
                          const std::string& round_type);
    bool LoadCurrentRound();
    void AddRoundShare(const MinerId miner, const double effort);
    void AddRoundSharePPLNS(const MinerId miner, const double effort);
    RoundCloseRes CloseRound(uint32_t& block_id, const BlockSubmission& submission, const double fee);
    void ResetRoundEfforts();

    inline Round GetChainRound() const { return round; };

    void PushPendingShares()
    {
        std::scoped_lock round_lock(efforts_map_mutex);

        this->AddPendingShares(pending_shares);
        pending_shares.clear();
    }

   private:
    static constexpr std::string_view field_str = "RoundManager";
    const Logger logger{field_str};
    const std::string round_type;

#if PAYMENT_SCHEME == PAYMENT_SCHEME_PPLNS
    // use raw bytes, to pass directly to redis in one command
    std::vector<Share> pending_shares;
#endif

    std::mutex round_map_mutex;
    Round round;

    std::mutex efforts_map_mutex;
    efforts_map_t efforts_map;
};

#endif