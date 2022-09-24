#ifndef STRATUM_CLIENT_HPP_
#define STRATUM_CLIENT_HPP_
#include <fmt/core.h>
#include <simdjson.h>
#include <sys/socket.h>

#include <cstring>
#include <list>
#include <memory>
#include <set>
#include <unordered_set>

#include "hash_wrapper.hpp"
#include "utils.hpp"
#include "verushash/verus_hash.h"

class StratumClient
{
   public:
    StratumClient(const int64_t time, const double diff);

    double GetDifficulty() const { return current_diff; }
    double GetPendingDifficulty() const { return pending_diff; }
    bool GetIsAuthorized() const { return is_authorized; }
    bool GetIsPendingDiff() const { return is_pending_diff; }
    uint32_t GetShareCount() const { return share_count; }
    int64_t GetLastAdjusted() const { return last_adjusted; }

    // make sting_view when unordered map supports it
    const std::string& GetAddress() const { return address; }
    const std::string& GetFullWorkerName() const { return worker_full; }

    void SetPendingDifficulty(double diff)
    {
        std::scoped_lock lock(shares_mutex);
        is_pending_diff = true;
        pending_diff = diff;
        // last_adjusted = curTime;
    }

    void ActivatePendingDiff()
    {
        is_pending_diff = false;
        current_diff = pending_diff;
    }

    bool SetLastShare(uint32_t shareEnd, int64_t time)
    {
        std::scoped_lock lock(shares_mutex);

        last_share_time = time;
        share_count++;

        // checks for existance in O(1), fast duplicate check
        // sub 1 us!!!
        bool inserted = share_uset.insert(shareEnd).second;

        return inserted;
    }

    void ResetShareSet()
    {
        std::scoped_lock lock(shares_mutex);

        share_uset.clear();
    }

    void ResetShareCount()
    {
        std::scoped_lock lock(shares_mutex);
        share_count = 0;
    }

    // called after auth, before added to databse
    void SetAddress(std::string_view worker, std::string_view addr)
    {
        // add the hasher on authorization
        // copy the worker name (it will be destroyed)
        worker_full = std::string(worker);
        address = std::string(addr);
    }

    void SetAuthorized() { is_authorized = true; }

    std::list<std::unique_ptr<StratumClient>>::iterator it;

    const std::string_view extra_nonce_sv;
    const int64_t connect_time;
    bool disconnected = false;

   private:
    static uint32_t extra_nonce_counter;

    const uint32_t extra_nonce;

    int64_t last_adjusted;
    int64_t last_share_time;
    uint32_t share_count = 0;

    double current_diff;
    double pending_diff;
    bool is_authorized = false;
    bool is_pending_diff = false;

    char extra_nonce_hex[EXTRANONCE_SIZE * 2];
    // std::string current_job_id;
    std::string worker_full;
    std::string address;
    std::string ip;

    std::mutex shares_mutex;

    // for O(1) duplicate search
    // at the cost of a bit of memory, but much faster!
    std::unordered_set<uint32_t> share_uset;
};

#endif