#ifndef COIN_CONFIG_HPP
#define COIN_CONFIG_HPP
#include <string_view>
#include <vector>
#include "daemon_manager.hpp"

struct RedisConfig
{
    std::string host;
    uint8_t db_index;
    uint32_t hashrate_ttl_seconds;
};

struct MySqlConfig {
    std::string host;
    std::string db_name;
    std::string user;
    std::string pass;
};

struct StatsConfig
{
    uint32_t hashrate_interval_seconds;
    uint32_t effort_interval_seconds;
    uint32_t average_hashrate_interval_seconds;
    uint32_t mined_blocks_interval;
};

struct DifficultyConfig{
    double default_diff;
    double minimum_diff;
    double target_shares_rate; // per minute
    uint32_t retarget_interval;
};

struct CoinConfig
{
    std::string symbol;
    std::string pool_addr;
    double pow_fee;
    double pos_fee;
    uint16_t stratum_port;
    uint16_t control_port;
    std::vector<RpcConfig> rpcs;
    std::vector<RpcConfig> payment_rpcs;

    RedisConfig redis;
    MySqlConfig mysql;
    DifficultyConfig diff_config;
    StatsConfig stats;

    uint32_t socket_recv_timeout_seconds;
    uint32_t block_poll_interval;
    uint32_t payment_interval_seconds;
    int64_t min_payout_threshold;
};
#endif