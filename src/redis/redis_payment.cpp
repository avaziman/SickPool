#include "redis_manager.hpp"

using enum Prefix;

bool RedisManager::AddPayout(const PaymentInfo* payment)
{
    using namespace std::string_view_literals;

    int64_t curtime = GetCurrentTimeMs();
    {
        RedisTransaction tx(this);
        for (const auto& [addr, reward] : payment->rewards)
        {
            std::string reward_str = std::to_string(-1 * reward);
            std::string_view negative_reward_sv(reward_str);
            // std::string_view reward_sv = negative_reward_sv.substr(1);

            std::string solver_key = Format({key_names.solver, addr});
            UserPayment upayment;
            upayment.amount = reward;
            upayment.id = payment->id;
            upayment.time = curtime;
            memcpy(upayment.hash_hex, payment->td.hash_hex,
                   HASH_SIZE_HEX);

            AppendCommand(
                {"ZINCRBY"sv, key_names.solver_index_mature, negative_reward_sv, addr});

            AppendCommand({"HINCRBY"sv, solver_key, EnumName<MATURE_BALANCE>(),
                           negative_reward_sv});

            AppendCommand({"LPUSH"sv,
                           Format({solver_key, EnumName<PAYOUTS>()}),
                           std::string_view((char*)&upayment, sizeof(UserPayment))});
        }

        FinishedPayment finished;
        memcpy(finished.hash_hex, payment->td.hash_hex,
               HASH_SIZE_HEX);
        finished.id = payment->id;
        finished.total_paid_amount = payment->total_paid;
        finished.time_ms = curtime;
        finished.fee = payment->td.fee;
        finished.total_payees = payment->rewards.size();

        AppendCommand(
            {"LPUSH"sv, key_names.payout,
             std::string_view((char*)&finished, sizeof(FinishedPayment))});
    }
    return GetReplies();
}