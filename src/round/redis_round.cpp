#include "redis_round.hpp"
using enum Prefix;

PersistenceRound::PersistenceRound(const PersistenceLayer &pl)
    : PersistenceBlock(pl)
{
}

void PersistenceRound::AppendSetMinerEffort(
    sw::redis::Pipeline &pipe, std::string_view chain, std::string_view miner,
    double effort)
{
    pipe.zadd(key_names.round_efforts, miner, effort);
}

void PersistenceRound::AppendSetRoundInfo(sw::redis::Pipeline& pipe, std::string_view field, double val)
{
    pipe.hset(key_names.round_stats, field, std::to_string(val));
}

// TODO: add total effort on share update
// bool PersistenceRound::SetEffortStats(const efforts_map_t &miner_stats_map,
//                                       const double total_effort,
//                                       std::unique_lock<std::mutex> stats_mutex)
// {
//     std::scoped_lock redis_lock(rc_mutex);

//     auto pipe =redis->pipeline(false);
//     for (const auto &[miner_id, miner_effort] : miner_stats_map)
//     {
//         AppendSetMinerEffort(pipe, key_names.coin, std::to_string(miner_id),
//                              EnumName<POW>(), miner_effort);
//     }

//     AppendSetRoundInfo(pipe, EnumName<TOTAL_EFFORT>(), total_effort);
//     stats_mutex.unlock();
//     pipe.exec();

//     return true;
//     // return GetReplies();
// }

// bool PersistenceRound::GetMinerEfforts(efforts_map_t &efforts,
//                                        std::string_view chain,
//                                        std::string_view type)
// {
//     using namespace std::string_view_literals;
//     using enum Prefix;

//     auto reply =
//         Command({"HGETALL"sv, Format({key_names.round_efforts, chain));

//     for (int i = 0; i < reply->elements; i += 2)
//     {
//         std::string id_str(reply->element[i]->str, reply->element[i]->len);

//         double effort = std::strtod(reply->element[i + 1]->str, nullptr);

//         MinerId id;
//         auto [_, err] = std::from_chars(id_str.data(),
//                                         id_str.data() + id_str.size(), id, 16);

//         assert(err == std::errc{);

//         efforts[id] = effort;
//     }
//     return true;
// }

void PersistenceRound::GetCurrentRound(Round *rnd, std::string_view chain,
                                       std::string_view type)
{
    std::string round_info_key = key_names.round_stats;

    auto pipe2 = redis->pipeline(false);

    std::optional<std::string> total_effortd =
       redis->hget(round_info_key, EnumName<TOTAL_EFFORT>());
    
    if (!total_effortd)
    {
        auto pipe =redis->pipeline(false);
        AppendSetRoundInfo(pipe, EnumName<TOTAL_EFFORT>(), 0);
        pipe.exec();

        return GetCurrentRound(rnd, chain, type);
    }

    std::optional<std::string> start_timed =
       redis->hget(round_info_key, EnumName<START_TIME>());
    if (!start_timed)
    {
        auto pipe = redis->pipeline(false);

        auto curtime = GetCurrentTimeMs();
        AppendSetRoundInfo(pipe, EnumName<START_TIME>(), curtime);
        pipe.exec();

        return GetCurrentRound(rnd, chain, type);
    }

    std::optional<std::string> estimated_effortd =
       redis->hget(round_info_key, EnumName<Prefix::ESTIMATED_EFFORT>());

    rnd->round_start_ms = std::stoll(*start_timed);
    rnd->estimated_effort = std::stod(*estimated_effortd);
}

RoundCloseRes PersistenceRound::SetClosedRound(
    uint32_t &block_id, const BlockSubmission &submission,
    const round_shares_t &round_shares, int64_t time_ms)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);

    if (!AddBlockSubmission(block_id, submission))
        return RoundCloseRes::BAD_ADD_BLOCK;

    if (!AddRoundRewards(submission, round_shares))
        return RoundCloseRes::BAD_ADD_REWARDS;

    {
        // either close everything about the round or nothing
        // auto pipe =redis->transaction(false, false);
        auto pipe =redis->pipeline(false);

        AppendSetRoundInfo(pipe, EnumName<START_TIME>(),
                           static_cast<double>(time_ms));
        // reset round total effort
        AppendSetRoundInfo(pipe, EnumName<TOTAL_EFFORT>(), 0);

        AppendTsAdd(pipe, block_key_names.mined_block_number, time_ms, 1.0);
        AppendTsAdd(pipe, block_key_names.block_effort_percent, time_ms,
                    submission.effort_percent);

        // no need to reset miners efforts in PPLNS
        pipe.exec();
    }
    // if (!GetReplies()) return RoundCloseRes::BAD_UPDATE_ROUND;

    return RoundCloseRes::OK;
}

bool PersistenceRound::SetNewBlockStats(std::string_view chain, uint32_t height,
                                        double target_diff)
{
    std::scoped_lock lock(rc_mutex);
    auto pipe =redis->pipeline(false);
    AppendSetRoundInfo(pipe, EnumName<ESTIMATED_EFFORT>(), target_diff);
    AppendUpdateBlockHeight(pipe, height);
    pipe.publish(block_key_names.block, "NEW BLOCK");

    pipe.exec();
    return true;
    // return GetReplies();
}

// manual binary search :)
// std::pair<std::span<Share>, redis_unique_ptr> PersistenceRound::GetLastNShares(
//     double diff, double n)
// {
//     std::unique_lock lock(rc_mutex);

//     redis_unique_ptr res = Command({"STRLEN", key_names.round_shares);
//     lock.unlock();

//     const size_t len = res->integer / sizeof(Share);
//     size_t low = 0;
//     size_t high = len;
//     ssize_t mid;

//     // we need the shares from the latest to the last one which sums is
//     // bigger or equal to diff - n
//     double target = diff - n;
//     double share_progress;

//     do
//     {
//         mid = std::midpoint(low, high);
//         auto [share, resp] = GetSharesBetween(mid, mid + 1);

//         if (share.empty())
//         {
//             return std::make_pair(std::span<Share>(), redis_unique_ptr());
//         }

//         share_progress = share.front().diff;
//         if (share_progress > target)
//         {
//             high = mid - 1;
//         }
//         else if (share_progress < target)
//         {
//             low = mid + 1;
//         }
//         else
//         {
//             mid++;
//             break;
//         }

//     } while (low < high);

//     size_t share_count = len - mid;
//     lock.lock();
//     res = Command({"GETRANGE", key_names.round_shares,
//                    std::to_string(-static_cast<ssize_t>(share_count) *
//                                   static_cast<ssize_t>(sizeof(Share))),
//                    std::to_string(-1));

//     std::string_view shares_sv(res->str, res->len);
//     if (shares_sv.size() != share_count * sizeof(Share))
//     {
//         return std::make_pair(std::span<Share>(), redis_unique_ptr());
//     }

//     // remove the unneccessary shares
//     auto res2 = Command({"SET", key_names.round_shares, shares_sv);

//     return std::make_pair(
//         std::span<Share>(reinterpret_cast<Share *>(res->str), share_count),
//         std::move(res));
// }

std::vector<Share>
PersistenceRound::GetSharesBetween(ssize_t start, ssize_t end)
{
    std::scoped_lock _(rc_mutex);

    ssize_t start_index = start * sizeof(Share);
    ssize_t end_index = end * sizeof(Share) - 1;
    if (end < 0) end_index = end;  // we want till the end

    std::optional<std::string> res =
       redis->getrange(key_names.round_shares,
                 start_index, end_index);

    if (!res)
    {
        return std::vector<Share>{};
    }

    return *reinterpret_cast<std::vector<Share> *>((*res).data());
}

void PersistenceRound::AddPendingShares(const std::vector<Share> &pending_shares)
{
    std::unique_lock rc_lock(rc_mutex);

    std::string_view sv(reinterpret_cast<const char *>(pending_shares.data()),
                        pending_shares.size() * sizeof(Share));
   redis->append(key_names.round_shares, sv);

    double diff_sum = 0;
    for (const Share &share : pending_shares)
    {
        diff_sum += share.diff;
    }

    int remove_count = 0;
    double first_share_value = 0.0;
    double rem_sum = 0;

    rc_lock.unlock();
    while (rem_sum < diff_sum)
    {
        auto rem_shares =
            GetSharesBetween(remove_count, remove_count + 10);

        for (Share share : rem_shares)
        {
            if (rem_sum + share.diff > diff_sum)
            {
                first_share_value = diff_sum - rem_sum;
                rem_sum += first_share_value;
                break;
            }
            rem_sum += share.diff;

            remove_count++;
        }
    }


    //// remove the unneccessary shares
    auto keep_shares = GetSharesBetween(-1, -1 - remove_count);

    sv = std::string_view(reinterpret_cast<const char *>(keep_shares.data()),
          keep_shares.size() * sizeof(Share));

    if (first_share_value != 0.0)
    {
        keep_shares[0].diff = first_share_value;
    }

    rc_lock.lock();
   redis->set(key_names.round_shares, sv);
}