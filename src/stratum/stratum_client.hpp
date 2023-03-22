#ifndef STRATUM_CLIENT_HPP_
#define STRATUM_CLIENT_HPP_
#include <fmt/core.h>
#include <simdjson.h>

#include <array>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "config_vrsc.hpp"
#include "difficulty_manager.hpp"
#include "hash_wrapper.hpp"
#include "stats.hpp"
#include "utils.hpp"
#include "verushash/verus_hash.h"

struct StringHash
{
    using is_transparent = void;  // enables heterogenous lookup

    std::size_t operator()(std::string_view sv) const
    {
        std::hash<std::string_view> hasher;
        return hasher(sv);
    }
};

class StratumClient : public VarDiff
{
   public:
    explicit StratumClient(const int64_t time, const double diff,
                           const double rate, uint32_t retarget_interval);

    double GetDifficulty() const { return current_diff; }
    std::optional<double> GetPendingDifficulty() const { return pending_diff; }
    bool GetHasAuthorized() const { return !authorized_workers.empty(); }

    std::optional<FullId> GetAuthorizedId(std::string_view worker_name) const
    {
        if (auto it = authorized_workers.find(worker_name);
            it != authorized_workers.end())
        {
            return std::optional<FullId>{(*it).second};
        }
        return std::optional<FullId>{};
    }

    void SetPendingDifficulty(double diff)
    {
        auto curtime = GetCurrentTimeMs();
        std::scoped_lock lock(shares_mutex);
        pending_diff = diff;
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

        uint64_t share_time = time - last_share_time;
        last_share_time = time;

        // checks for existance in O(1), fast duplicate check
        // sub 1 us!!!
        bool inserted = share_uset.insert(shareEnd).second;

        this->Add(share_time);

        lock.unlock();
        HandleAdjust(time);

        return inserted;
    }

    void HandleAdjust(uint64_t time)
    {
        if (double new_diff = this->Adjust(current_diff, time, last_adjusted);
            new_diff != 0.0)
        {
            SetPendingDifficulty(new_diff);
        }
    }

    void ResetShareSet()
    {
        std::scoped_lock lock(shares_mutex);
        share_uset.clear();
    }

    void AuthorizeWorker(const FullId full_id, std::string_view worker_name,
                         worker_map::iterator worker_it)
    {
        authorized_workers.try_emplace(std::string{worker_name}, full_id);

        this->stats_it = worker_it;
    }

    const int64_t connect_time;

    const uint32_t extra_nonce = extra_nonce_counter++;
    const std::array<char, 8> extra_nonce_hex{Hexlify(extra_nonce)};
    const std::string_view extra_nonce_sv{extra_nonce_hex.data(),
                                          sizeof(extra_nonce_hex)};

    std::list<std::unique_ptr<StratumClient>>::iterator it;
    worker_map::iterator stats_it;

   private:
    static uint32_t extra_nonce_counter;

    uint64_t last_adjusted;
    uint64_t last_share_time;

    double current_diff;
    std::optional<double> pending_diff;

    // std::string current_job_id;

    FullId id = FullId(0, 0);
    std::string worker_full;

    // support multiple workers for multiple addresses
    std::unordered_map<std::string, FullId, StringHash, std::equal_to<>>
        authorized_workers;

    std::mutex shares_mutex;

    // for O(1) duplicate search
    // at the cost of a bit of memory, but much faster!
    std::unordered_set<uint32_t> share_uset;
};

#endif