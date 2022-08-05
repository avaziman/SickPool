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
    bool GeneratePayout(RoundManager* round_manager,
                        int64_t time_now);

    void GeneratePayoutTx(
        std::vector<uint8_t>& bytes,
        const std::vector<std::pair<std::string, PayeeInfo>>& rewards);

    static int payout_age_seconds;
    static int64_t minimum_payout_threshold;

    bool payment_included = false;
    PaymentTx payment_tx;
    VerusTransaction tx = VerusTransaction(TXVERSION, 0, true, TXVERSION_GROUP);

    void CheckPayment();

   private:
    void ResetPayment();
    RedisManager* redis_manager;
    DaemonManager* daemon_manager;
    std::string pool_addr;
    // block id -> block height, maturity time, pending to be paid
    simdjson::ondemand::parser parser;
    };
#endif