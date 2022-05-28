#ifndef SHARE_HPP_
#define SHARE_HPP_
#include <vector>
#include <string_view>

#include "job.hpp"

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

class BlockSubmission
{
   public:
    BlockSubmission(ShareResult shareRes, std::string workerFull, int64_t time,
                    Job* job)
        : shareRes(shareRes),
          timeMs(time),
          job(job),
          height(job->GetHeight()),
          miner(workerFull.substr(0, workerFull.find('.'))),
          worker(workerFull.substr(workerFull.find('.') + 1, workerFull.size()))
    {
        Hexlify(hashHex, shareRes.HashBytes.data(), shareRes.HashBytes.size());
    }
    Job* job;
    const uint32_t height;
    const ShareResult shareRes;
    const std::string miner;
    const std::string worker;  // separated
    const int64_t timeMs;      // ms percision
    char hashHex[64];
};

#endif