#ifndef BLOCK_SUBMISSION_HPP
#define BLOCK_SUBMISSION_HPP

#include <cinttypes>
#include <cstring>
#include <string>

#include "stats/round.hpp"
#include "shares/share.hpp"
#include "jobs/verus_job.hpp"

#pragma pack(push, 1)
struct BlockSubmission
{
   public:
    BlockSubmission(const std::string_view chainsv,
                    const std::string_view workerFull, const job_t* job,
                    const ShareResult& shareRes, const Round& chainRound,
                    const int64_t time, const int32_t number)
        : blockReward(job->GetBlockReward()),
          timeMs(time),
          durationMs(time - chainRound.round_start_ms),
          height(job->GetHeight()),
          number(number),
          difficulty(shareRes.Diff),
          effortPercent(chainRound.pow / job->GetEstimatedShares() * 100.f)
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

    const int32_t confirmations = 0;  // up to 100, changed in database
    const int64_t blockReward;             
    const int64_t timeMs;             // ms percision
    const int64_t durationMs;         // ms percision
    const uint32_t height;
    const uint32_t number;
    const double difficulty;
    const double effortPercent;
    unsigned char chain[8];
    unsigned char miner[ADDRESS_LEN];
    unsigned char worker[MAX_WORKER_NAME_LEN];  // separated
    unsigned char hashHex[HASH_SIZE_HEX];
};
#pragma pack(pop)

// don't pack

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