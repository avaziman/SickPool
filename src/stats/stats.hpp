#ifndef STATS_HPP
#define STATS_HPP
#include <unordered_map>

#include "round.hpp"

// thought about hashing the address to produce shorter identifier for storage efficiency but it neglects the cryptocurrency's intent and requires another hash to addr mapping...
#pragma pack(push, 1)
struct Share
{
    std::byte addr[ADDRESS_LEN];
    double progress;

    operator std::string() const { return std::string((char*)addr); }
 };
#pragma pack(pop)

enum class BadDiff
{
    STALE_SHARE_DIFF = -1,
    INVALID_SHARE_DIFF = -2
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

    uint32_t connection_count = 0;

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

struct string_hash
{
    using transparent_key_equal = std::equal_to<>;  // Pred to use
    using hash_type = std::hash<std::string_view>;  // just a helper local type
    size_t operator()(std::string_view txt) const { return hash_type{}(txt); }
    size_t operator()(const std::string& txt) const { return hash_type{}(txt); }
    size_t operator()(const char* txt) const { return hash_type{}(txt); }
};

typedef std::unordered_map<std::string, WorkerStats>
    worker_map;

typedef std::unordered_map<std::string, MinerStats> miner_map;

typedef std::unordered_map<std::string, Round>
    round_map_t;

// miner -> effort
typedef std::unordered_map<std::string, double>
    efforts_map_t;


#endif