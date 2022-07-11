#ifndef SHARE_HPP_
#define SHARE_HPP_
#include <string_view>
#include <vector>

#include "jobs/job.hpp"
#include "jobs/verus_job.hpp"
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
    ShareCode code;
    std::string message;
    double difficulty;
    // uint256 takes vector as param
    std::vector<uint8_t> hash_bytes = std::vector<uint8_t>(32);
};

#endif
//TODO:
// add round_start redis key, load it at startup for duration, update on round close
//add confirmations left field for block submission decr with bitfield