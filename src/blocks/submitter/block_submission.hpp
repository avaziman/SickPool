#ifndef BLOCK_SUBMISSION_HPP_
#define BLOCK_SUBMISSION_HPP_
#include <array>
#include <cstdint>
#include <string>
#include "redis_interop.hpp"

struct BlockSubmission
{
    uint32_t id;
    uint32_t miner_id;
    uint32_t worker_id;

    uint8_t block_type;
    uint8_t chain;
    uint64_t reward;
    uint64_t time_ms;
    uint64_t duration_ms;
    uint32_t height;
    double difficulty;
    double effort_percent;
    std::array<uint8_t, 32> hash_bin;
};

struct BlockOverview {
    uint32_t id;
    uint32_t height;
    BlockStatus last_status;
    std::string hash;
};

#endif