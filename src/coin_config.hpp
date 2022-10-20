#ifndef COIN_CONFIG_HPP
#define COIN_CONFIG_HPP
#include <string_view>
#include <vector>
#include "daemon_manager.hpp"

struct RedisConfig
{
    uint16_t redis_port;
    uint32_t hashrate_ttl_seconds;
};

struct StatsConfig
{
    uint32_t hashrate_interval_seconds;
    uint32_t effort_interval_seconds;
    uint32_t average_hashrate_interval_seconds;
    uint32_t mined_blocks_interval;
    uint32_t diff_adjust_seconds;
};

struct CoinConfig
{
    std::string pool_addr;
    double pow_fee;
    double pos_fee;
    double default_difficulty;
    double target_shares_rate;
    double minimum_difficulty;
    uint16_t stratum_port;
    uint16_t control_port;
    std::vector<RpcConfig> rpcs;

    RedisConfig redis;
    StatsConfig stats;

    uint32_t socket_recv_timeout_seconds;
    uint32_t payment_interval_seconds;
    uint32_t min_payout_threshold;
};
#endif