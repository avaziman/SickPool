#ifndef STRATUM_CLIENT_HPP_
#define STRATUM_CLIENT_HPP_
#include <cstring>
#include <set>
#include <unordered_set>
#include <memory>

#include "../crypto/hash_wrapper.hpp"
#include "../crypto/utils.hpp"
#include "../crypto/verushash/verus_hash.h"

class StratumClient
{
   public:
    StratumClient(const int sock, const int64_t time, const double diff)
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

    int GetSock() const { return sockfd; }
    double GetDifficulty() const { return current_diff; }
    const char* GetExtraNonce() const { return extra_nonce_str; }
    uint32_t GetShareCount() const { return share_count; }
    std::string_view GetFullWorkerName() const
    {
        return std::string_view(worker_full);
    }
    std::string_view GetAddress() const { return std::string_view(address); }
    int64_t GetLastAdjusted() const { return last_adjusted; }
    uint8_t* GetBlockheaderBuff() { return block_header; }
    void ResetShareCount() { share_count = 0; }

    void SetDifficulty(double diff, int64_t curTime)
    {
        last_diff = current_diff;
        current_diff = diff;
        last_adjusted = curTime;
    }

    bool SetLastShare(uint32_t shareEnd, int64_t time)
    {
        last_share_time = time;
        share_count++;

        // checks for existance in O(1), fast duplicate check
        // sub 1 us!!!
        bool inserted = share_uset.insert(shareEnd).second;

        return inserted;
    }

    // called after auth
    void HandleAuthorized(std::string_view worker, std::string_view addr)
    {
        // add the hasher on authorization
        // copy the worker name (it will be destroyed)
        share_uset = std::unordered_set<uint32_t>();
        worker_full = std::string(worker);
        address = std::string(addr);
#if POOL_COIN <= COIN_VRSC
        this->verusHasher = std::make_unique<CVerusHashV2>(CVerusHashV2(SOLUTION_VERUSHHASH_V2_2));
#endif
    }

#if POOL_COIN <= COIN_VRSC
    CVerusHashV2* GetHasher() { return verusHasher.get(); }
#endif
   private:
    int sockfd;
    const int64_t connect_time;
    int64_t last_adjusted;
    int64_t last_share_time;
    uint32_t extra_nonce;
    uint32_t share_count = 0;
    double current_diff;
    double last_diff;

    char extra_nonce_str[9];
    std::string worker_full;
    std::string address;

    // needs to be thread-specific to allow simultanious processing
    uint8_t block_header[BLOCK_HEADER_SIZE];

    // for O(log N) duplicate search
    // 2^32 (1 in 4B) chance of false duplicate
    // no need to save the entire hash
    // absolutely no need to save the entire block header like snomp :)
    // std::set<uint32_t> share_set;

    // for O(1) duplicate search
    // at the cost of a bit of memory, but much faster!
    std::unordered_set<uint32_t> share_uset;

    // the hasher is thread-specific
    // so we need to store it in client so we only need to init once
#if POOL_COIN <= COIN_VRSC
    std::unique_ptr<CVerusHashV2> verusHasher;
#endif
};

#endif