#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "crypto/base58.h"
#include "crypto/hash_wrapper.hpp"
#include "crypto/verus_transaction.hpp"
#include "crypto/verushash/arith_uint256.h"
#include "crypto/verushash/verus_hash.h"
#include "daemon/daemon_rpc.hpp"
#include "logger.hpp"
#include "sock_addr.hpp"
#include "stratum/stratum_server.hpp"

// #define CONFIG_PATH                                                   \
//     "//home/sickguy/Documents/Projects/SickPool/server/config/coins/" \
//     "VRSC.json"
// #define CONFIG_PATH_VRSC \
//     "C:\\projects\\pool\\pool-server\\config\\coins\\VRSC.json"

void ParseCoinConfig(const simdjson::padded_string& json, CoinConfig& cnfg);

int main(int argc, char** argv)
{
    std::cout << "Block Submission Size: " << sizeof(BlockSubmission);
    Logger::Log(LogType::Info, LogField::Config, "Starting SickPool!");

    Logger::Log(LogType::Info, LogField::Config, "Static config:");
    Logger::Log(LogType::Info, LogField::Config, "Coin symbol: %s",
                COIN_SYMBOL);

    Logger::Log(LogType::Info, LogField::Config, "Loading dynamic config...");

    CoinConfig coinConfig;
    try
    {
        simdjson::padded_string json = simdjson::padded_string::load(argv[1]);
        ParseCoinConfig(json, coinConfig);

        Logger::Log(LogType::Info, LogField::Config, "Coin config loaded:");
        Logger::Log(LogType::Info, LogField::Config, "Pool address: %s",
                    coinConfig.pool_addr.c_str());
        Logger::Log(LogType::Info, LogField::Config, "Redis port: %d",
                    coinConfig.redis_port);
        Logger::Log(LogType::Info, LogField::Config, "PoW fee: %f",
                    coinConfig.pow_fee);
        Logger::Log(LogType::Info, LogField::Config, "PoS fee: %f",
                    coinConfig.pos_fee);
        Logger::Log(LogType::Info, LogField::Config, "Hashrate retention: %ds",
                    coinConfig.hashrate_interval_seconds);
        Logger::Log(LogType::Info, LogField::Config, "Hashrate ttl: %ds",
                    coinConfig.hashrate_ttl_seconds);
        Logger::Log(LogType::Info, LogField::Config, "Effort retention: %ds",
                    coinConfig.effort_interval_seconds);

        StratumServer stratumServer(coinConfig);
        stratumServer.StartListening();
    }
    catch (std::runtime_error e)
    {
        Logger::Log(LogType::Critical, LogField::Config, "START-UP ERROR: %s.",
                    e.what());
        return EXIT_FAILURE;
    }
    return EXIT_FAILURE;  // should never finish!
}

void AssignJson(const char* name, std::string& obj,
                simdjson::ondemand::document& doc)
{
    try
    {
        std::string_view sv = doc[name].get_string();

        obj = std::string(sv);
    }
    catch (...)
    {
        throw std::runtime_error(std::string("Invalid or no \"") + name +
                                 "\" (string) variable in config file");
    }
}

template <typename T>
void AssignJson(const char* name, T& obj, simdjson::ondemand::document& doc)
{
    try
    {
        obj = doc[name].get<T>();
    }
    catch (...)
    {
        throw std::runtime_error(std::string("Invalid or no \"") + name +
                                 "\" () variable in config file");
    }
}

void ParseCoinConfig(const simdjson::padded_string& json, CoinConfig& cnfg)
{
    simdjson::ondemand::parser confParser;
    simdjson::ondemand::document configDoc = confParser.iterate(json);

    AssignJson("stratum_port", cnfg.stratum_port, configDoc);
    AssignJson("control_port", cnfg.control_port, configDoc);
    AssignJson("redis_port", cnfg.redis_port, configDoc);
    AssignJson("hashrate_interval_seconds", cnfg.hashrate_interval_seconds,
               configDoc);
    AssignJson("effort_interval_seconds", cnfg.effort_interval_seconds,
               configDoc);
    AssignJson("average_hashrate_interval_seconds",
               cnfg.average_hashrate_interval_seconds, configDoc);
    AssignJson("hashrate_ttl", cnfg.hashrate_ttl_seconds, configDoc);
    AssignJson("socket_recv_timeout_seconds", cnfg.socket_recv_timeout_seconds,
               configDoc);
    AssignJson("diff_adjust_seconds", cnfg.diff_adjust_seconds, configDoc);
    AssignJson("pow_fee", cnfg.pow_fee, configDoc);
    AssignJson("pos_fee", cnfg.pos_fee, configDoc);
    AssignJson("default_diff", cnfg.default_diff, configDoc);
    AssignJson("target_shares_rate", cnfg.target_shares_rate, configDoc);
    AssignJson("pool_addr", cnfg.pool_addr, configDoc);

    try
    {
        auto rpcs = configDoc["rpcs"].get_array();
        for (auto rpc : rpcs)
        {
            RpcConfig rpcConf;
            std::string_view host_sv = rpc["host"].get_string();
            std::string_view auth_sv = rpc["auth"].get_string();
            rpcConf.host = std::string(host_sv);
            rpcConf.auth = std::string(auth_sv);
            cnfg.rpcs.push_back(rpcConf);
        }
    }
    catch (...)
    {
        throw std::runtime_error(
            std::string("Invalid or no \"") + "rpcs" +
            "\"; ([string host, string auth]) variable in config file");
    }
}