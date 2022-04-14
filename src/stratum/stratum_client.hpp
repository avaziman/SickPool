#ifndef STRATUM_CLIENT_HPP_
#define STRATUM_CLIENT_HPP_
#include <cstring>
#include <set>

#include "../crypto/hash_wrapper.hpp"
#include "../crypto/utils.hpp"
#include "../crypto/verushash/verus_hash.h"

class StratumClient
{
   public:
    StratumClient(const int sock, const std::time_t time, const double diff)
        : sockfd(sock),
          connect_time(time),
          last_adjusted(time),
          last_share_time(time),
          current_diff(diff),
          last_diff(diff)
    {
        extra_nonce = 10;
        ToHex(extra_nonce_str, extra_nonce);
        extra_nonce_str[8] = 0;
    }

    ~StratumClient() { delete verusHasher; }

    int GetSock() { return sockfd; }
    double GetDifficulty() { return current_diff; }
    void SetDifficulty(double diff, std::time_t curTime)
    {
        last_diff = current_diff;
        current_diff = diff;
        last_adjusted = curTime;
    }

    const char* GetExtraNonce() { return extra_nonce_str; }
    uint32_t GetShareCount() { return share_count; }
    std::string GetWorkerName() { return worker_full; }
    void ResetShareCount() { share_count = 0; }
    std::time_t GetLastAdjusted() { return last_adjusted; }

    bool SetLastShare(uint32_t shareEnd, std::time_t time)
    {
        last_share_time = time;
        share_count++;

        // checks for existance in O(log N), fast duplicate check
        bool inserted = share_set.insert(shareEnd).second;
        return inserted;
    }

    void SetWorkerFull(std::string_view worker)
    {
        // add the hasher on authorization
        worker_full = std::string(worker);
#if POOL_COIN <= COIN_VRSC
        this->verusHasher = new CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);
#endif
    }

#if POOL_COIN <= COIN_VRSC
    CVerusHashV2* GetHasher() { return verusHasher; }
#endif
   private:
    int sockfd;
    const std::time_t connect_time;
    uint32_t extra_nonce;
    std::time_t last_adjusted;
    std::time_t last_share_time;
    uint32_t share_count = 0;
    double current_diff;
    double last_diff;

    char extra_nonce_str[9];
    std::string worker_full;

    // for O(log N) duplicate search
    // 2^32 (1 in 4B) chance of false duplicate
    // no need to save the entire hash
    // absolutely no need to save the entire block header like snomp :)
    std::set<uint32_t> share_set;

    // the hasher is thread-specific
    // so we need to store it in client so we only need to init once
#if POOL_COIN <= COIN_VRSC
    CVerusHashV2* verusHasher;
#endif
};

#endif