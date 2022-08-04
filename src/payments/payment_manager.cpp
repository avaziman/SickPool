#include "payment_manager.hpp"

int PaymentManager::payout_age_seconds;
int64_t PaymentManager::minimum_payout_threshold;

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

// returns true if new rewards have aged and ready to be paid
bool PaymentManager::CheckAgingRewards(RoundManager* round_manager,
                                       int64_t time_now_ms)
{
    bool is_updated = false;
    while (time_now_ms >
           aging_blocks.back().matued_at_ms + payout_age_seconds * 1000)
    {
        is_updated = true;

        auto aged_block = aging_blocks.back();

        // add new payments
        std::vector<std::pair<std::string, RewardInfo>> rewards;
        round_manager->LoadMatureRewards(rewards, aged_block.id);

        for (const auto& [addr, reward_info] : rewards)
        {
            auto it = pending_payments.find(addr);
            if (it == pending_payments.end())
            {
                // load script
                // std::string script_hex = redis_manager->hgets(
                //     fmt::format("solver:{}", addr), SCRIPT_PUB_KEY_KEY);
                PendingPayment pending_payment;
                pending_payment.amount = 0;
                // pending_payment.script_pub_key.resize(script_hex.size() / 2);
                // Unhexlify(pending_payment.script_pub_key.data(),
                //           script_hex.data(), script_hex.size());

                it = pending_payments.emplace(addr, pending_payment).first;
            }

            pending_payments[addr].amount += reward_info.reward;
        }
        AppendAgedRewards(aged_block, rewards);

        // tx.AddInput

        aging_blocks.pop_back();
    }
    return is_updated;
}

void PaymentManager::AppendAgedRewards(
    const AgingBlock& aged_block,
    const std::vector<std::pair<std::string, RewardInfo>>& rewards)
{
    // coinbase index is always 0, leave unsigned, sign on daemon later...
    tx.AddInput(aged_block.coinbase_txid, 0, std::vector<uint8_t>(),
                UINT32_MAX);

    int64_t total_spent = 0;
    for (auto& [addr, reward_info] : rewards)
    {
        // check if passed threshold
        auto& pending_payment = pending_payments[addr];
        if (pending_payment.amount < reward_info.settings.threshold ||
            pending_payment.amount == 0)
        {
            continue;
        }

        // leave 1000 bytes for coinbase tx and change
        if (tx.GetTxLen() + 1000 > MAX_BLOCK_SIZE)
        {
            break;
        }

        tx.AddOutput(pending_payment.script_pub_key, pending_payment.amount);

        total_spent += pending_payment.amount;
    }

    int64_t change = aged_block.reward - total_spent;
    tx.AddP2PKHOutput(pool_addr, change);
}

void PaymentManager::AddMatureBlock(uint32_t block_id,
                                    const char* coinbase_txid,
                                    int64_t mature_time_ms, int64_t reward)
{
    AgingBlock block;
    block.id = block_id;
    block.matued_at_ms = mature_time_ms;
    block.reward = reward;
    memcpy(block.coinbase_txid, coinbase_txid, HASH_SIZE_HEX);

    aging_blocks.push_back(block);
}