#include "payment_manager.hpp"

Logger<PaymentManager::field_str> PaymentManager::logger;

int PaymentManager::payout_age_seconds;
int64_t PaymentManager::minimum_payout_threshold;
int64_t PaymentManager::last_payout_ms;
uint32_t PaymentManager::payment_counter = 0;

PaymentManager::PaymentManager(RedisManager* rm, daemon_manager_t* dm,
                               const std::string& pool_addr, int payout_age_s,
                               int64_t min_threshold)
    : redis_manager(rm), daemon_manager(dm), pool_addr(pool_addr)
{
    PaymentManager::payout_age_seconds = payout_age_s;
    PaymentManager::minimum_payout_threshold = min_threshold;
}

inline void AddShare(round_shares_t& miner_shares, double& score_sum,
                     const double n, const double score,
                     const int64_t block_reward, const std::string& addr)
{
    const double share_ratio = score / n;
    const auto reward =
        static_cast<int64_t>(share_ratio * static_cast<double>(block_reward));

    score_sum += score;

    miner_shares[addr].reward += reward;
    miner_shares[addr].share += share_ratio;
    miner_shares[addr].effort += 1;
}

bool PaymentManager::GetRewardsPPLNS(round_shares_t& miner_shares,
                                     const std::span<Share> shares,
                                     const int64_t block_reward, const double n)
{
    double score_sum = 0;
    miner_shares.reserve(shares.size());
    for (size_t i = shares.size() - 1; i > 1; i--)
    {
        const double score = shares[i].progress - shares[i - 1].progress;
        
        AddShare(miner_shares, score_sum, n, score, block_reward,
                 shares[i]);
    }

    if (score_sum >= n){
        return false;
    }

    const double last_score = n - score_sum;

    AddShare(miner_shares, score_sum, n, last_score, block_reward,
             shares[0]);

    return true;
}

bool PaymentManager::GetRewardsPROP(round_shares_t& miner_shares,
                                    const int64_t block_reward,
                                    const efforts_map_t& miner_efforts,
                                    double total_effort, double fee)
{
    int64_t substracted_reward =
        static_cast<int64_t>(static_cast<double>(block_reward) * (1.f - fee));

    if (total_effort == 0)
    {
        logger.Log<LogType::Critical>("Round effort is 0!");
        return false;
    }
    miner_shares.reserve(miner_efforts.size());

    for (auto& [addr, effort] : miner_efforts)
    {
        RoundShare round_share;
        round_share.effort = effort;
        round_share.share = round_share.effort / total_effort;
        round_share.reward = static_cast<int64_t>(
            round_share.share * static_cast<double>(substracted_reward));
        miner_shares.try_emplace(addr, round_share);

        logger.Log<LogType::Info>(
            "Miner round share: {}, effort: {}, share: {}, reward: "
            "{}, total effort: {}",
            addr, round_share.effort, round_share.share, round_share.reward,
            total_effort);
    }

    return true;
}

// bool PaymentManager::GeneratePayout(RoundManager* round_manager,
//                                     int64_t time_now_ms)
// {
//     // add new payments
//     std::vector<uint8_t> payout_bytes;
//     payees_info_t rewards;
//     // round_manager->LoadUnpaidRewards(rewards);

//     for (const auto& [addr, info] : rewards)
//     {
//         logger.Log<LogType::Info>(
//                     "Pending reward to {} -> {}, threshold: {}.", addr,
//                     info.amount, info.settings.threshold);
//     }

//     if (!GeneratePayoutTx(payout_bytes, rewards))
//     {
//         return false;
//     }

//     //TODO: createrawtransaction

//     char bytes_hex[payout_bytes.size() * 2];
//     Hexlify(bytes_hex, payout_bytes.data(), payout_bytes.size());
//     std::string_view unfunded_rawtx(bytes_hex, sizeof(bytes_hex));

//     FundRawTransactionRes fund_res;
//     // 0 fees
//     if (!daemon_manager->FundRawTransaction(fund_res, parser, unfunded_rawtx,
//     0, pool_addr))
//         {
//             logger.Log<LogType::Info>(
//                         "Failed to fund rawtransaction. {}", unfunded_rawtx);
//             return false;
//         }

//     SignRawTransactionRes sign_res;
//     if (!daemon_manager->SignRawTransaction(sign_res, parser, fund_res.hex))
//     {
//         logger.Log<LogType::Info>(
//                     "Failed to sign rawtransaction.");
//         return false;
//     }

//     pending_payment->raw_transaction_hex = sign_res.hex;
//     pending_payment->td =
//     TransactionData(pending_payment->raw_transaction_hex);

//     return true;
// }

// void PaymentManager::UpdatePayouts(RoundManager* round_manager,
//                                    int64_t curtime_ms)
// {
//     if (finished_payment)
//     {
//         // will be freed after here, since we moved
//         auto complete_tx = std::move(finished_payment);

//         redis_manager->AddPayout(complete_tx.get());

//         last_payout_ms = curtime_ms;
//         payment_counter++;

//         finished_payment.release();

//         logger.Log<
//             LogType::Info>(
//             "Payment has been included in block and added to database!");
//     }

//     // make sure there is no currently pending payment or an unprocessed
//     // finished payment
//     if (!pending_payment.get() && !finished_payment.get() &&
//         curtime_ms > last_payout_ms + (payout_age_seconds * 1000))
//     {
//         if (GeneratePayout(round_manager, curtime_ms))
//         {
//             logger.Log<
//                 LogType::Info>(
//                 "Generated payout txid: {}, data: {}",
//                 std::string_view(pending_payment->td.hash_hex,
//                 HASH_SIZE_HEX), pending_payment->td.data_hex);
//         }
//         else
//         {
//             pending_payment.release();
//             logger.Log<LogType::Info>(
//                         "Failed to generate payout Tx.");
//             // std::string_view(pending_tx->td.hash_hex, HASH_SIZE_HEX),
//             // pending_tx->td.data_hex);
//         }
//     }
// }