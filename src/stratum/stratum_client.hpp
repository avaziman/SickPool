#ifndef STRATUM_CLIENT_HPP_
#define STRATUM_CLIENT_HPP_
#include <simdjson.h>
#include <sys/socket.h>

#include <cstring>
#include <memory>
#include <set>
#include <unordered_set>

#include "config.hpp"
#include "hash_wrapper.hpp"
#include "utils.hpp"
#include "verushash/verus_hash.h"

#define REQ_BUFF_SIZE (1024 * 32)
#define REQ_BUFF_SIZE_REAL (REQ_BUFF_SIZE - simdjson::SIMDJSON_PADDING)
class StratumClient
{
   public:
    StratumClient(const int sock, const std::string& ip, const int64_t time, const double diff);

    int GetSock() const { return sockfd; }
    double GetDifficulty() const { return current_diff; }
    double GetPendingDifficulty() const { return pending_diff; }
    bool GetIsAuthorized() const { return is_authorized; }
    bool GetIsPendingDiff() const { return is_pending_diff; }
    uint32_t GetShareCount() const { return share_count; }
    int64_t GetLastAdjusted() const { return last_adjusted; }
    std::string_view GetExtraNonce() const { return extra_nonce_str; }
    std::string_view GetIp() const { return ip; }

    // make sting_view when unordered map supports it
    const std::string& GetAddress() const { return address; }
    const std::string& GetFullWorkerName() const { return worker_full; }

    void SetPendingDifficulty(double diff)
    {
        std::scoped_lock lock(shares_mutex);
        is_pending_diff = true;
        pending_diff = diff;
        share_count = 0;
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

    // called after auth, before added to databse
    void SetAddress(std::string_view worker, std::string_view addr)
    {
        // add the hasher on authorization
        // copy the worker name (it will be destroyed)
        worker_full = std::string(worker);
        address = std::string(addr);
    }

    void SetAuthorized()
    {
        is_authorized = true;
    }
    char req_buff[REQ_BUFF_SIZE];
    std::size_t req_pos = 0;
    std::mutex epoll_mutex;

   private:
    static uint32_t extra_nonce_counter;

    int sockfd;
    const int64_t connect_time;
    int64_t last_adjusted;
    int64_t last_share_time;
    uint32_t extra_nonce;
    uint32_t share_count = 0;
    
    double current_diff;
    double pending_diff;
    bool is_authorized;
    bool is_pending_diff;

    std::string extra_nonce_str;
    std::string worker_full;
    std::string address;
    std::string ip;

    std::mutex shares_mutex;

    // for O(1) duplicate search
    // at the cost of a bit of memory, but much faster!
    std::unordered_set<uint32_t> share_uset;

};

#endif