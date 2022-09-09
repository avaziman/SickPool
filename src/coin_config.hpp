#ifndef COIN_CONFIG_HPP
#define COIN_CONFIG_HPP
#include <string_view>
#include <vector>

struct RpcConfig{
    std::string host;
    std::string auth;
};

struct CoinConfig
{
    std::string pool_addr;
    double pow_fee;
    double pos_fee;
    double default_diff;
    double target_shares_rate;
    double minimum_difficulty;
    int64_t stratum_port;
    int64_t control_port;
    int64_t redis_port;
    std::vector<RpcConfig> rpcs;

    int64_t hashrate_interval_seconds;
    int64_t effort_interval_seconds;
    int64_t average_hashrate_interval_seconds;  
    int64_t hashrate_ttl_seconds;
    int64_t diff_adjust_seconds;
    int64_t socket_recv_timeout_seconds;
    int64_t payment_interval_seconds;
    int64_t min_payout_threshold;
};
#endif