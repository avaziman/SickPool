#ifndef PAYMENT_MANAGER_HPP
#define PAYMENT_MANAGER_HPP
#include <deque>
#include <unordered_map>

#include "daemon_manager_t.hpp"
#include "logger.hpp"
#include "redis_manager.hpp"
#include "round_manager.hpp"
#include "round_share.hpp"
#include "stats.hpp"
#include "transaction.hpp"
#include "block_template.hpp"

class RoundManager;
class RedisManager;

class PaymentManager
{
   public:
    PaymentManager(RedisManager* rm, daemon_manager_t* dm, const std::string& pool_addr, int payout_age_s,
                   int64_t minimum_payout_threshold);
    static bool GetRewardsProp(round_shares_t& miner_shares,
                               int64_t block_reward,
                               efforts_map_t& miner_efforts,
                               double total_effort, double fee);
    bool GeneratePayout(RoundManager* round_manager, int64_t time_now);

    static uint32_t payment_counter;
    static int payout_age_seconds;
    static int64_t last_payout_ms;
    static int64_t minimum_payout_threshold;

    std::unique_ptr<PaymentInfo> pending_payment;
    std::unique_ptr<PaymentInfo> finished_payment;

    void UpdatePayouts(RoundManager* round_manager, int64_t curtime_ms);

   private:
    bool GeneratePayoutTx(
        std::vector<uint8_t>& bytes,
        const std::vector<std::pair<std::string, PayeeInfo>>& rewards);
    // void ResetPayment();
    static Logger<LogField::PaymentManager> logger;
    RedisManager* redis_manager;
    daemon_manager_t* daemon_manager;
    std::string pool_addr;
    // block id -> block height, maturity time, pending to be paid
    simdjson::ondemand::parser parser;
};
#endif