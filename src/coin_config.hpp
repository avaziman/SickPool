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
    // std::string name;
    // std::string symbol;
    // std::string algo;
    std::string pool_addr;
    double pow_fee;
    double pos_fee;
    double default_diff;
    double target_shares_rate;
    int64_t stratum_port;
    std::string redis_host;
    std::vector<RpcConfig> rpcs;

    int64_t hashrate_interval_seconds;
    int64_t effort_interval_seconds;
    int64_t average_hashrate_interval_seconds;
    int64_t hashrate_ttl_seconds;
};
#endif