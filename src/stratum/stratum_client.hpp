#ifndef STRATUM_CLIENT_HPP_
#define STRATUM_CLIENT_HPP_
#include <fmt/core.h>
#include <simdjson.h>
#include <sys/socket.h>

#include <cstring>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>

#include "difficulty_manager.hpp"
#include "hash_wrapper.hpp"
#include "stats.hpp"
#include "utils.hpp"
#include "verushash/verus_hash.h"

class StratumClient
{
   public:
    explicit StratumClient(const int64_t time, const double diff,
                           const double rate);

    double GetDifficulty() const { return current_diff; }
    double GetPendingDifficulty() const { return pending_diff.value(); }
    bool GetIsAuthorized() const { return is_authorized; }
    bool GetIsPendingDiff() const { return pending_diff.has_value(); }
    uint32_t GetShareCount() const { return share_count; }
    int64_t GetLastAdjusted() const { return last_adjusted; }

    // make sting_view when unordered map supports it
    std::string_view GetAddress() const { return address; }
    std::string_view GetWorkerName() const { return worker_name; }
    std::string_view GetFullWorkerName() const { return worker_full; }
    FullId GetId() const { return id; }

    void SetPendingDifficulty(double diff)
    {
        auto curtime = GetCurrentTimeMs();
        std::scoped_lock lock(shares_mutex);
        pending_diff = diff;
        share_count = 0;
        last_adjusted = curtime;
    }

    void ActivatePendingDiff()
    {
        current_diff = pending_diff.value();
        pending_diff.reset();
    }

    bool SetLastShare(uint32_t shareEnd, uint64_t time)
    {
        std::unique_lock lock(shares_mutex);

        auto share_time = time - last_share_time;
        last_share_time = time;
        share_count++;

        // checks for existance in O(1), fast duplicate check
        // sub 1 us!!!
        bool inserted = share_uset.insert(shareEnd).second;

        var_diff.Add(share_time);

        if (double new_diff = var_diff.Adjust(current_diff); new_diff != 0.0)
        {
            lock.unlock();
            SetPendingDifficulty(new_diff);
        }

        return inserted;
    }

    void ResetShareSet()
    {
        std::scoped_lock lock(shares_mutex);
        share_uset.clear();
    }

    void SetAuthorized(const FullId full_id, std::string&& workerfull,
                       const worker_map::iterator& worker_it)
    {
        is_authorized = true;
        this->id = full_id;

        this->worker_full = std::move(workerfull);
        std::string_view worker_full_sv(worker_full);

        auto dot = worker_full_sv.find('.');
        this->address = worker_full_sv.substr(0, dot);
        this->worker_name =
            worker_full_sv.substr(dot + 1, worker_full_sv.size() - 1);
        this->stats_it = worker_it;
    }

    const std::string_view extra_nonce_sv;
    const int64_t connect_time;

    std::list<std::unique_ptr<StratumClient>>::iterator it;
    worker_map::iterator stats_it;

   private:
    static uint32_t extra_nonce_counter;

    const uint32_t extra_nonce;
    char extra_nonce_hex[EXTRANONCE_SIZE * 2];

    uint64_t last_adjusted;
    uint64_t last_share_time;
    uint32_t share_count = 0;

    VarDiff var_diff;
    double current_diff;
    std::optional<double> pending_diff;
    bool is_authorized = false;

    // std::string current_job_id;

    FullId id;
    std::string worker_full;
    // both point to worker_full
    std::string_view address;
    std::string_view worker_name;

    std::mutex shares_mutex;

    // for O(1) duplicate search
    // at the cost of a bit of memory, but much faster!
    std::unordered_set<uint32_t> share_uset;
};

#endif