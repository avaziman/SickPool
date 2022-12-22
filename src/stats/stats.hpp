#ifndef STATS_HPP
#define STATS_HPP
#include <fmt/core.h>

#include <unordered_map>
#include <list>
#include "redis_interop.hpp"
#include "round.hpp"
// thought about hashing the address to produce shorter identifier for storage
// efficiency but it neglects the cryptocurrency's intent and requires another
// hash to addr mapping...

enum class BadDiff
{
    STALE_SHARE_DIFF = -1,
    INVALID_SHARE_DIFF = -2
};

struct NetworkStats
{
    double network_hr;
    double difficulty;
};

struct WorkerStats
{
    // derived from the below
    double average_hashrate = 0.0;
    double interval_hashrate = 0.0;

    uint32_t interval_valid_shares = 0;
    uint32_t interval_stale_shares = 0;
    uint32_t interval_invalid_shares = 0;

    double average_hashrate_sum = 0.0;
    double current_interval_effort = 0.0;

    // uint32_t connection_count = 0;

    inline void ResetInterval()
    {
        this->current_interval_effort = 0;
        this->interval_valid_shares = 0;
        this->interval_stale_shares = 0;
        this->interval_invalid_shares = 0;
    }
};

struct MinerStats : public WorkerStats
{
    uint32_t worker_count = 0;
};

struct Id32
{
    /*const*/ uint32_t id;
    char hex[sizeof(id) * 2];
    std::string_view GetHex() const
    {
        return std::string_view(hex, sizeof(hex));
    }

    explicit(false) Id32(uint32_t i) : id(i)
    {
        fmt::format_to_n(this->hex, sizeof(this->hex), "{:08x}", id);
    }

    bool operator==(const Id32& i) const { return this->id == i.id; }
};

using MinerId = uint32_t;
using WorkerId = uint32_t;

using MinerIdHex = Id32;
using WorkerIdHex = Id32;

struct WorkerFullId
{
    /*const*/ MinerIdHex miner_id;
    /*const*/ WorkerIdHex worker_id;

    char hex[sizeof(miner_id.hex) + sizeof(worker_id.hex)];
    std::string_view GetHex() const
    {
        return std::string_view(hex, sizeof(hex));
    }

    explicit WorkerFullId(uint32_t mi, uint32_t wi)
        : miner_id(mi), worker_id(wi)
    {
        fmt::format_to_n(
            hex, sizeof(hex), "{}{}",
            std::string_view(miner_id.hex, sizeof(miner_id.hex)),
            std::string_view(worker_id.hex, sizeof(worker_id.hex)));
    }

    bool operator==(const WorkerFullId& i) const
    {
        return miner_id == i.miner_id && worker_id == i.worker_id;
    }
};

template <>
struct std::hash<Id32>
{
    std::size_t operator()(const Id32& s) const noexcept
    {
        return std::hash<uint32_t>{}(s.id);
    }
};

template <>
struct std::hash<WorkerFullId>
{
    std::size_t operator()(const WorkerFullId& s) const noexcept
    {
        return std::hash<Id32>{}(s.miner_id) << 32 |
               std::hash<Id32>{}(s.worker_id);
    }
};

#pragma pack(push, 1)
struct Share
{
    const MinerId miner_id;
    const double progress;
};
#pragma pack(pop)

using worker_map = std::list<std::pair<WorkerFullId, WorkerStats>>;

using miner_map = std::unordered_map<MinerIdHex, MinerStats>;

using round_map_t = std::unordered_map<std::string, Round>;

// miner -> effort
using efforts_map_t = std::unordered_map<MinerIdHex, double>;

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