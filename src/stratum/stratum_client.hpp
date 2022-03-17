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
    StratumClient(int sock, std::time_t time, double diff)
        : sockfd(sock),
          connect_time(time),
          last_adjusted(time),
          last_share_time(time),
          current_diff(diff),
          last_diff(diff)
    {
#if POOL_COIN == COIN_VRSCTEST
        this->verusHasher = new CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);
        unsigned char buf1[32] = {0};
        unsigned char buf2[32] = {0};
        HashWrapper::VerushashV2b2(buf1, buf2, 32, this->verusHasher);
#endif

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
    void ResetShareCount() { share_count = 0; }
    std::time_t GetLastAdjusted() { return last_adjusted; }

    bool SetLastShare(uint32_t shareEnd, std::time_t time)
    {
        last_share_time = time;
        share_count++;

        bool inserted = share_set.insert(shareEnd).second;
        return inserted;
    }

#if POOL_COIN == COIN_VRSCTEST
    CVerusHashV2* GetHasher() { return verusHasher; }
#endif
   private:
    int sockfd;
    uint32_t extra_nonce;
    std::time_t connect_time;
    std::time_t last_adjusted;
    std::time_t last_share_time;
    uint32_t share_count = 0;
    char extra_nonce_str[9];
    double current_diff;
    double last_diff;

    // for O(log N) duplicate search
    // 2^32 (1 in 4B) chance of false duplicate
    // no need to save the entire hash
    // absolutely no need to save the entire block header like snomp :)
    std::set<uint32_t> share_set;

    // the keys used in the hash are thread-specific,
    // so we need to store it in client so we only need to generate keys once
#if POOL_COIN == COIN_VRSCTEST
    CVerusHashV2* verusHasher;
#endif
};

#endif