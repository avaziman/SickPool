#ifndef STRATUM_CLIENT_HPP_
#define STRATUM_CLIENT_HPP_
#include <simdjson.h>

#include <cstring>
#include <memory>
#include <set>
#include <unordered_set>

#include "../crypto/hash_wrapper.hpp"
#include "../crypto/utils.hpp"
#include "../crypto/verushash/verus_hash.h"

#define REQ_BUFF_SIZE (1024 * 32)
#define REQ_BUFF_SIZE_REAL (REQ_BUFF_SIZE - simdjson::SIMDJSON_PADDING)
class StratumClient
{
   public:
    StratumClient(const int sock, const int64_t time, const double diff);

    int GetSock() const { return sockfd; }
    double GetDifficulty() const { return current_diff; }
    std::string_view GetExtraNonce() const
    {
        return std::string_view(extra_nonce_str);
    }
    uint32_t GetShareCount() const { return share_count; }
    bool GetIsAuthorized() const { return is_authorized; }
    std::string_view GetAddress() const { return std::string_view(address); }
    int64_t GetLastAdjusted() const { return last_adjusted; }
    uint8_t* GetBlockheaderBuff() { return block_header; }
    std::string_view GetFullWorkerName() const
    {
        return std::string_view(worker_full);
    }

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
        share_uset = std::unordered_set<uint32_t>();

#if POW_ALGO == POW_ALGO_VERUSHASH
        this->verusHasher = CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);
#endif
    }

    simdjson::ondemand::parser* GetParser() { return &parser; }

#if POW_ALGO == POW_ALGO_VERUSHASH
    CVerusHashV2* GetHasher() { return &verusHasher; }
#endif
   private:
    static uint32_t extra_nonce_counter;

    int sockfd;
    const int64_t connect_time;
    int64_t last_adjusted;
    int64_t last_share_time;
    uint32_t extra_nonce;
    uint32_t share_count = 0;
    double current_diff;
    double last_diff;
    bool is_authorized;

    std::string extra_nonce_str;
    std::string worker_full;
    std::string address;

    // each parser can only process one request at a time,
    // so we need a parser per client
    simdjson::ondemand::parser parser =
        simdjson::ondemand::parser(REQ_BUFF_SIZE);

    // needs to be thread-specific to allow simultanious processing
    uint8_t block_header[BLOCK_HEADER_SIZE];

    // for O(1) duplicate search
    // at the cost of a bit of memory, but much faster!
    std::unordered_set<uint32_t> share_uset;

    // the hasher is thread-specific
    // so we need to store it in client so we only need to init once
#if POW_ALGO == POW_ALGO_VERUSHASH
    CVerusHashV2 verusHasher;
#endif
};

#endif