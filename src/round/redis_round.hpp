    #ifndef REDIS_ROUND_HPP_
#define REDIS_ROUND_HPP_
#include <mutex>

#include "redis_manager.hpp"
class RedisRound : public RedisManager
{
   public:
    explicit RedisRound(const RedisManager &rm) : RedisManager(rm) {}

    std::pair<std::span<Share>, redis_unique_ptr> GetLastNShares(double progress,
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


            if(share.empty()) {
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
            return std::make_pair(
                std::span<Share>(),
                redis_unique_ptr());
        }


        // remove the unneccessary shares
        auto res2 = Command({"SET", key_names.round_shares, shares_sv});

        return std::make_pair(
            std::span<Share>(reinterpret_cast<Share *>(res->str), share_count),
            std::move(res));
    }

    std::pair<std::span<Share>, redis_unique_ptr> GetSharesBetween(ssize_t start,
                                                          ssize_t end)
    {
        std::scoped_lock _(rc_mutex);

        ssize_t start_index = start * sizeof(Share);
        ssize_t end_index = end * sizeof(Share) - 1;
        if (end < 0) end_index = end; // we want till the end

        auto res = Command({"GETRANGE", key_names.round_shares,
                            std::to_string(start_index),
                            std::to_string(end_index)});

        return std::make_pair(
            std::span<Share>(reinterpret_cast<Share *>(res->str), res->len / sizeof(Share)), std::move(res));
    }

    void AddPendingShares(const std::vector<Share>& pending_shares)
    {
        std::scoped_lock _(rc_mutex);

        std::string_view sv(
            reinterpret_cast<const char *>(pending_shares.data()),
            pending_shares.size() * sizeof(Share));
        Command({"APPEND", key_names.round_shares, sv});
    }

    void AppendSetMinerEffort(std::string_view chain, std::string_view miner,
                              std::string_view type, double effort);

    void AppendAddRoundShares(std::string_view chain,
                              const BlockSubmission *submission,
                              const round_shares_t &miner_shares);

    bool SetClosedRound(std::string_view chain, std::string_view type,
                    const ExtendedSubmission *submission,
                    const round_shares_t &round_shares, int64_t time_ms);
    void GetCurrentRound(Round *rnd, std::string_view chain,
                           std::string_view type);

    bool GetMinerEfforts(efforts_map_t &efforts, std::string_view chain,
                           std::string_view type);

    bool SetEffortStats(const efforts_map_t &miner_stats_map,
                           const double total_effort,
                           std::unique_lock<std::mutex> stats_mutex);
};

#endif