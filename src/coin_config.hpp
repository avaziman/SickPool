#ifndef COIN_CONFIG_HPP
#define COIN_CONFIG_HPP

struct RpcConfig
{
    std::string host;
    std::string auth;
};

struct CoinConfig
{
    std::string name;
    std::string symbol;
    std::string algo;

    u_short stratum_port;

    double pow_fee;
    double pos_fee;

    double default_diff;

    RpcConfig rpcs[4];

    std::string pool_addr;
    std::string redis_host;
};
#endif