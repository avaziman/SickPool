#include "redis_manager.hpp"

bool RedisManager::AddPayout(const PendingPayment* payment)
{
    using namespace std::string_view_literals;
    int64_t curtime = GetCurrentTimeMs();
    {
        RedisTransaction tx(this);
        for (const auto& [addr, reward] : payment->info.rewards)
        {
            std::string reward_str = std::to_string(-1 * reward);
            std::string_view negative_reward_sv(reward_str);
            std::string_view reward_sv = negative_reward_sv.substr(1);

            std::string solver_key = fmt::format("solver:{}", addr);
            UserPayment upayment;
            upayment.amount = reward;
            upayment.id = payment->id;
            upayment.time = curtime;
            memcpy(upayment.hash_hex, payment->info.tx_hash_hex.data(),
                   HASH_SIZE_HEX);

            AppendCommand({"ZINCRBY"sv,
                           fmt::format("solver-index:{}", MATURE_BALANCE_KEY),
                           negative_reward_sv, addr});

            AppendCommand({"HINCRBY"sv, solver_key, MATURE_BALANCE_KEY,
                           negative_reward_sv});

            AppendCommand({"LPUSH"sv,
                           fmt::format("{}:{}", solver_key, PAYOUTS_KEY),
                           reward_sv});
        }

        FinishedPayment finished;
        memcpy(finished.hash_hex, payment->info.tx_hash_hex.data(),
               HASH_SIZE_HEX);
        finished.id = payment->id;
        finished.total_paid_amount = payment->info.total_paid;
        finished.time_ms = curtime;
        finished.fee = payment->info.fee;
        finished.total_payees = payment->info.rewards.size();

        AppendCommand(
            {"LPUSH"sv, PAYOUTS_KEY,
             std::string_view((char*)&finished, sizeof(FinishedPayment))});
    }
    return GetReplies();
}