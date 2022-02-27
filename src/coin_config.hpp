#ifndef COIN_CONFIG_HPP
#define COIN_CONFIG_HPP
struct RpcConfig{
    std::string host;
    std::string auth;
};

struct CoinConfig
{
    std::string name;
    std::string symbol;
    std::string algo;
    std::string pool_addr;
    double pow_fee;
    double pos_fee;
    double default_diff;
    double target_shares_rate;
    ushort stratum_port;
    std::string redis_host;
    RpcConfig rpcs[4];
};
#endif