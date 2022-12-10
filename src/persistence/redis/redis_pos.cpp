#include "redis_manager.hpp"

// bool RedisManager::AddStakingPoints(std::string_view chain, int64_t duration_ms)
// {
//     using namespace std::string_view_literals;

//     std::scoped_lock lock(rc_mutex);

//     auto stakers_reply = (redisReply *)redisCommand(
//         rc, "HGETALL %b:balance:mature", chain.data(), chain.size());

//     redisReply *reply;

//     {
//         RedisTransaction load_tx(this);

//         double pool_staking_days = 0.f;
//         for (int i = 0; i < stakers_reply->elements; i += 2)
//         {
//             std::string_view addr(stakers_reply->element[i]->str,
//                                   stakers_reply->element[i]->len);

//             int64_t balance =
//                 std::strtoll(stakers_reply->element[i + 1]->str, nullptr, 10);

//             // how many days we have staked 1 vrsc
//             double staking_days = ((double)balance / 1e8) *
//                                   ((double)duration_ms / (1000 * 60 * 60 * 24));
//             pool_staking_days += staking_days;

//             logger.Log<LogType::Info>(
//                         "Staker {} has earned {} staking minutes this round.",
//                         addr, staking_days);

//             AppendCommand({"HINCRBY"sv,
//                            fmt::format("{}:pos:round-effort", chain), addr,
//                            std::to_string(staking_days)});
//         }

//         AppendCommand({"HINCRBY"sv, fmt::format("{}:pos:round-effort", chain),
//                        PrefixKey<Prefix::TOTAL_EFFORT>(), std::to_string(pool_staking_days)});
//     }
//     return GetReplies();
// }

// bool RedisManager::GetPosPoints(
//     std::vector<std::pair<std::string, double>> &stakers,
//     std::string_view chain)
// {
//     auto stakers_reply = (redisReply *)redisCommand(
//         rc, "HGETALL %b:balance:mature", chain.data(), chain.size());

//     if (!stakers_reply)
//     {
//         return false;
//     }

//     stakers.resize(stakers_reply->elements / 2);

//     for (int i = 0; i < stakers_reply->elements; i += 2)
//     {
//         std::string addr(stakers_reply->element[i]->str,
//                          stakers_reply->element[i]->len);

//         double points =
//             std::strtod(stakers_reply->element[i + 1]->str, nullptr);

//         stakers.emplace_back(addr, points);
//     }
//     freeReplyObject(stakers_reply);

//     return true;
// }