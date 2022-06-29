#ifndef BLOCK_SUBMISSION_HPP
#define BLOCK_SUBMISSION_HPP

#include <string>
#include <cinttypes>
#include <cstring>
#include "verus_job.hpp"
#include "./crypto/utils.hpp"

struct ImmatureSubmission
{
   public:
    ImmatureSubmission(int64_t time, const std::string& hash, const std::string& subId)
        : timeMs(time), hashHex(hash), submission_id(subId)
    {
    }
     int64_t timeMs;
     std::string hashHex;
     std::string submission_id;
};

struct BlockSubmission
{
   public:
    BlockSubmission(std::string_view chainsv, const job_t* job,
                    const ShareResult& shareRes, std::string_view workerFull,
                    int64_t time, const Round& chainEffort, int32_t number)
        : blockReward(job->GetBlockReward()),
          timeMs(time),
          durationMs(time - chainEffort.round_start_ms),
          height(job->GetHeight()),
          number(number),
          difficulty(shareRes.Diff),
          effortPercent(chainEffort.pow / job->GetEstimatedShares() * 100.f),
          confirmations(0)
    {
        memset(miner, 0, sizeof(miner));
        memset(worker, 0, sizeof(worker));
        memset(chain, 0, sizeof(chain));

        std::size_t dotPos = workerFull.find('.');
        memcpy(miner, workerFull.data(), dotPos);
        memcpy(worker, workerFull.data() + dotPos + 1,
               workerFull.size() - dotPos - 1);
        memcpy(chain, chainsv.data(), chainsv.size());

        // reverse
        Hexlify((char*)hashHex, shareRes.HashBytes.data(),
                shareRes.HashBytes.size());
        ReverseHex((char*)hashHex, (char*)hashHex, HASH_SIZE_HEX);
    }

    const int32_t confirmations = 0;     // up to 100, changed in database
    int64_t blockReward;       // changed if rejected
    const int64_t timeMs;      // ms percision
    const int64_t durationMs;  // ms percision
    const uint32_t height;
    const uint32_t number;
    const double difficulty;
    const double effortPercent;
    unsigned char chain[8];
    unsigned char miner[ADDRESS_LEN];
    unsigned char worker[MAX_WORKER_NAME_LEN];  // separated
    unsigned char hashHex[HASH_SIZE_HEX];
};  //__attribute__((packed));
// don't pack for speed

/* block submission attributes are
    sortable:
    time/number
    reward
    difficulty
    effort
    duration
    (no need height because it will always be grouped by chains, so you can just
   filter by chain and sort by time/number).

   non-sortable (filterable):

    chain
    type (pow/pos)
    solver address
*/

#endif