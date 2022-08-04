#ifndef PAYMENT_MANAGER_HPP
#define PAYMENT_MANAGER_HPP
#include <deque>
#include <unordered_map>

#include "config.hpp"
#include "round_share.hpp"
#include "daemon_manager.hpp"
#include "logger.hpp"
#include "round_manager.hpp"
#include "stats.hpp"
#include "redis_manager.hpp"
#include "verus_transaction.hpp"

class RoundManager;
class RedisManager;

class PaymentManager
{
   public:
    PaymentManager(const std::string& pool_addr, int payout_age_s, int64_t minimum_payout_threshold);
    static bool GetRewardsProp(round_shares_t& miner_shares,
                               int64_t block_reward,
                               efforts_map_t& miner_efforts,
                               double total_effort, double fee);
    bool CheckAgingRewards(RoundManager* redis_manager, int64_t time_now);

    void AddMatureBlock(uint32_t block_id, const char* coinbase_txid,
                        int64_t mature_time_ms, int64_t reward);

    void AppendAgedRewards(
        const AgingBlock& aged_block,
        const std::vector<std::pair<std::string, RewardInfo>>& rewards);
    static int payout_age_seconds;
    static int64_t minimum_payout_threshold;

    VerusTransaction tx = VerusTransaction(TXVERSION, 0, true, TXVERSION_GROUP);
   private:
    RedisManager* redis_manager;
    std::string pool_addr;
    // block id -> block height, maturity time, pending to be paid
    std::unordered_map<std::string, PendingPayment> pending_payments;
    std::deque<AgingBlock> aging_blocks;
    simdjson::ondemand::parser parser;
};
#endif