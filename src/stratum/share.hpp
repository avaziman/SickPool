#ifndef SHARE_HPP_
#define SHARE_HPP_
#include <string_view>
#include <vector>

#include "job.hpp"
#include "verus_job.hpp"
struct Share
{
    std::string_view worker;
    std::string_view jobId;
    std::string_view time;
    std::string_view nonce2;
    std::string_view solution;
};

enum class ShareCode
{
    UNKNOWN = 20,
    JOB_NOT_FOUND = 21,
    DUPLICATE_SHARE = 22,
    LOW_DIFFICULTY_SHARE = 23,
    UNAUTHORIZED_WORKER = 24,
    NOT_SUBSCRIBED = 25,

    // non-standard
    VALID_SHARE = 30,
    VALID_BLOCK = 31,
};

struct ShareResult
{
    ShareResult() : HashBytes(32) {}
    ShareCode Code;
    std::string Message;
    double Diff;
    // uint256 takes vector as param
    std::vector<uint8_t> HashBytes;
};

// size 176 bytes
struct BlockSubmission
{
   public:
    BlockSubmission(std::string_view chainsv, const job_t* job,
                    const ShareResult& shareRes, std::string_view workerFull,
                    int64_t time, int64_t durMs,
                    int32_t number, double totalEffort)
        : blockReward(job->GetBlockReward()),
          timeMs(time),
          durationMs(durMs),
          height(job->GetHeight()),
          number(number),
          difficulty(shareRes.Diff),
          estimatedShares(job->GetEstimatedShares()),
          effortPercent(totalEffort / estimatedShares * 100.f)
    {
        memset(this, 0, sizeof(BlockSubmission));
        
        std::size_t dotPos = workerFull.find('.');
        memcpy(miner, workerFull.data(), dotPos);
        memcpy(worker, workerFull.data() + dotPos + 1,
               workerFull.size() - dotPos - 1);
        memcpy(chain, chainsv.data(), chainsv.size());

        Hexlify(hashHex, shareRes.HashBytes.data(),
                shareRes.HashBytes.size());
    }

    const int64_t blockReward;
    const int64_t timeMs;      // ms percision
    const int64_t durationMs;  // ms percision
    const uint32_t height;
    const uint32_t number;
    const double difficulty;
    const double estimatedShares;
    const double effortPercent;
    char chain[8];
    char miner[ADDRESS_LEN];
    char worker[MAX_WORKER_NAME_LEN];  // separated
    char hashHex[HASH_SIZE_HEX];
} __attribute__((packed));

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