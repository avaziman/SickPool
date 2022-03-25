#ifndef COIN_CONFIG_HPP
#define COIN_CONFIG_HPP
#include <string_view>
struct RpcConfig{
    std::string_view host;
    std::string_view auth;
};

struct CoinConfig
{
    // std::string_view name;
    // std::string_view symbol;
    // std::string_view algo;
    std::string_view pool_addr;
    double pow_fee;
    double pos_fee;
    double default_diff;
    double target_shares_rate;
    ushort stratum_port;
    std::string_view redis_host;
    std::vector<RpcConfig> rpcs;
};
#endif