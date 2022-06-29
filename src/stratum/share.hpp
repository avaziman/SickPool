#ifndef SHARE_HPP_
#define SHARE_HPP_
#include <string_view>
#include <vector>

#include "stats_manager.hpp"
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

#endif
//TODO:
// add round_start redis key, load it at startup for duration, update on round close
//add confirmations left field for block submission decr with bitfield