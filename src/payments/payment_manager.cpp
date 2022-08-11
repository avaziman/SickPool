#include "payment_manager.hpp"

int PaymentManager::payout_age_seconds;
int64_t PaymentManager::minimum_payout_threshold;
int64_t PaymentManager::last_payout_ms;
uint32_t PaymentManager::payment_counter = 0;

PaymentManager::PaymentManager(const std::string& pool_addr, int payout_age_s,
                               int64_t min_threshold)
    : pool_addr(pool_addr)
{
    PaymentManager::payout_age_seconds = payout_age_s;
    PaymentManager::minimum_payout_threshold = min_threshold;
}

bool PaymentManager::GetRewardsProp(round_shares_t& miner_shares,
                                    int64_t block_reward,
                                    efforts_map_t& miner_efforts,
                                    double total_effort, double fee)
{
    int64_t substracted_reward =
        static_cast<int64_t>(static_cast<double>(block_reward) * (1.f - fee));

    if (total_effort == 0)
    {
        Logger::Log(LogType::Critical, LogField::PaymentManager,
                    "Round effort is 0!");
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
        miner_shares.emplace_back(addr, round_share);

        Logger::Log(LogType::Info, LogField::PaymentManager,
                    "Miner round share: {}, effort: {}, share: {}, reward: "
                    "{}, total effort: {}",
                    addr, round_share.effort, round_share.share,
                    round_share.reward, total_effort);
    }

    return true;
}

bool PaymentManager::GeneratePayout(RoundManager* round_manager,
                                    int64_t time_now_ms)
{
    // add new payments
    std::vector<uint8_t> payout_bytes;
    std::vector<std::pair<std::string, PayeeInfo>> rewards;
    round_manager->LoadUnpaidRewards(rewards);

    if (rewards.empty())
    {
        return false;
    }

    GeneratePayoutTx(payout_bytes, rewards);

    char bytes_hex[payout_bytes.size() * 2];
    Hexlify(bytes_hex, payout_bytes.data(), payout_bytes.size());

    FundRawTransactionRes fund_res;
    if (!daemon_manager->FundRawTransaction(
            fund_res, parser, std::string_view(bytes_hex, sizeof(bytes_hex))))
    {
        return false;
    }

    pending_tx->info.raw_transaction_hex = fund_res.hex;

    // tx.AddInput

    return true;
}

void PaymentManager::GeneratePayoutTx(
    std::vector<uint8_t>& bytes,
    const std::vector<std::pair<std::string, PayeeInfo>>& rewards)
{
    // inputs will be added in fundrawtransaction
    // change will be auto added in fundrawtransaction
    for (auto& [addr, reward_info] : rewards)
    {
        // check if passed threshold
        if (reward_info.amount < reward_info.settings.threshold ||
            reward_info.amount == 0)
        {
            continue;
        }

        // leave 50000 bytes for funding inputs and change and other
        // transactions in the block
        if (pending_tx->tx.GetTxLen() + 50000 > MAX_BLOCK_SIZE)
        {
            break;
        }

        pending_tx->info.rewards.emplace_back(addr, reward_info.amount);
        pending_tx->tx.AddOutput(reward_info.script_pub_key, reward_info.amount);

        pending_tx->info.total_paid += reward_info.amount;
    }
    pending_tx->tx.GetBytes(bytes);
}

void PaymentManager::ResetPayment()
{
}

bool PaymentManager::ShouldIncludePayment(std::string_view prevblockhash)
{
    if (!pending_tx.get())
    {
        // no pending payment
        return false;
    }

    if (pending_tx->info.tx_hash_hex == prevblockhash)
    {
        // payment has been included in block
        payment_included = true;
        return false;
    }

    return true;
}

void PaymentManager::UpdatePayouts(RoundManager* round_manager,
                                   int64_t curtime_ms)
{
    if (payment_included)
    {   
        // will be freed after here, since we moved
        auto complete_tx = std::move(pending_tx);
        redis_manager->AddPayout(complete_tx.get());

        last_payout_ms = curtime_ms;
        payment_included = false;

        pending_tx = std::make_unique<PendingPayment>(payment_counter++);

        Logger::Log(LogType::Info, LogField::PaymentManager,
                    "Payment has been included in block and added to database!");
    }

    if (pending_tx.get() &&
        curtime_ms > last_payout_ms + (payout_age_seconds * 1000))
    {
        GeneratePayout(round_manager, curtime_ms);
    }
}