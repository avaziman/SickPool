#ifndef BLOCK_SUBMISSION_HPP
#define BLOCK_SUBMISSION_HPP

#include <cinttypes>
#include <cstring>
#include <string>

#include "static_config.hpp"
#include "shares/share.hpp"
#include "stats/round.hpp"
#include "stats.hpp"
#include "utils/hex_utils.hpp"

enum class BlockType : uint8_t
{
    POW = 0b1,
    PAYMENT = 0b100,
    POW_PAYMENT = POW | PAYMENT
};

#pragma pack(push, 1)
struct BlockSubmission
{
    const int32_t confirmations = 0;  // up to 100, changed in database
    const BlockType block_type;
    const int64_t block_reward;
    const int64_t time_ms;      // ms percision
    const int64_t duration_ms;  // ms percision
    const uint32_t height;
    const uint32_t number;
    const double difficulty;
    const double effort_percent;
    const uint8_t chain;
    const MinerId miner_id;
    const WorkerId worker_id;
    const std::array<char, 64> hash_hex;
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