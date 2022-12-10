#include "redis_round.hpp"
using enum Prefix;

void PersistenceRound::AppendSetMinerEffort(std::string_view chain,
                                      std::string_view miner,
                                      std::string_view type, double effort)
{
    AppendHset(key_names.round_efforts,
               miner, std::to_string(effort));
}

bool PersistenceRound::SetEffortStats(const efforts_map_t &miner_stats_map,
                                     const double total_effort,
                                     std::unique_lock<std::mutex> stats_mutex)
{
    std::scoped_lock redis_lock(rc_mutex);

    for (const auto &[miner_id, miner_effort] : miner_stats_map)
    {
        AppendSetMinerEffort(key_names.coin, miner_id.GetHex(), EnumName<POW>(),
                             miner_effort);
    }

    AppendSetMinerEffort(key_names.coin, EnumName<TOTAL_EFFORT>(), EnumName<POW>(),
                         total_effort);
    stats_mutex.unlock();

    return GetReplies();
}

bool PersistenceRound::GetMinerEfforts(efforts_map_t &efforts,
                                  std::string_view chain, std::string_view type)
{
    using namespace std::string_view_literals;
    using enum Prefix;

    auto reply = Command({"HGETALL"sv, Format({key_names.round_efforts, chain})});

    for (int i = 0; i < reply->elements; i += 2)
    {
        std::string id_str(reply->element[i]->str, reply->element[i]->len);

        double effort = std::strtod(reply->element[i + 1]->str, nullptr);

        MinerId id;
        auto [_, err] = std::from_chars(id_str.data(), id_str.data() + id_str.size(), id, 16);

        assert(err == std::errc{});

        efforts[MinerIdHex(id)] = effort;
    }
    return true;
}

void PersistenceRound::GetCurrentRound(Round *rnd, std::string_view chain,
                                   std::string_view type)
{
    std::string round_effort_key =
        Format({chain, type, EnumName<ROUND_EFFORT>()});

    std::string total_effort_str =
        hget(round_effort_key, EnumName<TOTAL_EFFORT>());

    std::string start_time_str = hget(round_effort_key, EnumName<START_TIME>());

    std::string estimated_effort_str =
        hget(round_effort_key, EnumName<Prefix::ESTIMATED_EFFORT>());

    rnd->round_start_ms = strtoll(start_time_str.c_str(), nullptr, 10);
    rnd->estimated_effort = strtod(estimated_effort_str.c_str(), nullptr);
    rnd->total_effort = strtod(total_effort_str.c_str(), nullptr);
}

void PersistenceRound::AppendAddRoundRewards(std::string_view chain,
                                       const BlockSubmission& submission,
                                       const round_shares_t &miner_shares)
{
    using namespace std::string_view_literals;

    for (const auto &[miner_id, round_share] : miner_shares)
    {
        AppendCommand(
            {"HSET"sv,
             Format({key_names.reward_immature,
                     std::to_string(submission.number)}),
             miner_id.GetHex(),
             std::string_view(reinterpret_cast<const char *>(&round_share),
                              sizeof(RoundReward))});

        AppendCommand({"HINCRBY"sv, Format({key_names.solver, miner_id.GetHex()}),
                       EnumName<Prefix::IMMATURE_BALANCE>(),
                       std::to_string(round_share.reward)});
    }
}

bool PersistenceRound::SetClosedRound(std::string_view chain, std::string_view type,
                              const BlockSubmission& submission,
                              const round_shares_t &round_shares,
                              int64_t time_ms)
{
    using namespace std::string_view_literals;

    std::scoped_lock lock(rc_mutex);
    {
        // either close everything about the round or nothing
        RedisTransaction close_round_tx(this);

        AppendAddBlockSubmission(submission);
        // AppendAddRoundRewards(chain, submission, round_shares);
        // set round start time
        AppendSetMinerEffort(chain, EnumName<START_TIME>(), type,
                             static_cast<double>(time_ms));
        // reset round total effort
        AppendSetMinerEffort(chain, EnumName<TOTAL_EFFORT>(), type, 0);

        // reset miners efforts
        for (auto &[miner_id, _] : round_shares)
        {
            AppendSetMinerEffort(chain, miner_id.GetHex(), type, 0);
        }
    }

    return GetReplies();
}

bool PersistenceRound::SetNewBlockStats(std::string_view chain, double target_diff)
{
    std::scoped_lock lock(rc_mutex);

    // AppendTsAdd(key_name.hashrate_network, curtime_ms, net_est_hr);

    AppendSetMinerEffort(chain, EnumName<ESTIMATED_EFFORT>(), "pow",
                         target_diff);
    return GetReplies();
}

// manual binary search :)
std::pair<std::span<Share>, redis_unique_ptr> PersistenceRound::GetLastNShares(double progress,
                                                             double n)
{
    std::unique_lock lock(rc_mutex);

    redis_unique_ptr res = Command({"STRLEN", key_names.round_shares});
    lock.unlock();

    const size_t len = res->integer / sizeof(Share);
    size_t low = 0;
    size_t high = len;
    ssize_t mid;

    // we need the shares from the latest to the last one which sums is
    // bigger or equal to progress - n
    double target = progress - n;
    double share_progress;

    do
    {
        mid = std::midpoint(low, high);
        auto [share, resp] = GetSharesBetween(mid, mid + 1);

        if (share.empty())
        {
            return std::make_pair(std::span<Share>(), redis_unique_ptr());
        }

        share_progress = share.front().progress;
        if (share_progress > target)
        {
            high = mid - 1;
        }
        else if (share_progress < target)
        {
            low = mid + 1;
        }
        else
        {
            mid++;
            break;
        }

    } while (low < high);

    size_t share_count = len - mid;
    lock.lock();
    res = Command({"GETRANGE", key_names.round_shares,
                   std::to_string(-static_cast<ssize_t>(share_count) *
                                  static_cast<ssize_t>(sizeof(Share))),
                   std::to_string(-1)});

    std::string_view shares_sv(res->str, res->len);
    if (shares_sv.size() != share_count * sizeof(Share))
    {
        return std::make_pair(std::span<Share>(), redis_unique_ptr());
    }

    // remove the unneccessary shares
    auto res2 = Command({"SET", key_names.round_shares, shares_sv});

    return std::make_pair(
        std::span<Share>(reinterpret_cast<Share *>(res->str), share_count),
        std::move(res));
}

std::pair<std::span<Share>, redis_unique_ptr> PersistenceRound::GetSharesBetween(ssize_t start,
                                                               ssize_t end)
{
    std::scoped_lock _(rc_mutex);

    ssize_t start_index = start * sizeof(Share);
    ssize_t end_index = end * sizeof(Share) - 1;
    if (end < 0) end_index = end;  // we want till the end

    auto res =
        Command({"GETRANGE", key_names.round_shares,
                 std::to_string(start_index), std::to_string(end_index)});

    if(!res || !res->str)
    {
        return std::make_pair(std::span<Share>(), std::move(res));
    }

    return std::make_pair(std::span<Share>(reinterpret_cast<Share *>(res->str),
                                           res->len / sizeof(Share)),
                          std::move(res));
}

void PersistenceRound::AddPendingShares(const std::vector<Share> &pending_shares)
{
    std::scoped_lock _(rc_mutex);

    std::string_view sv(reinterpret_cast<const char *>(pending_shares.data()),
                        pending_shares.size() * sizeof(Share));
    Command({"APPEND", key_names.round_shares, sv});
}