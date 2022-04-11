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

#define CONFIG_PATH                                                   \
    "//home/sickguy/Documents/Projects/SickPool/server/config/coins/" \
    "VRSC.json"
// #define CONFIG_PATH_VRSC \
//     "C:\\projects\\pool\\pool-server\\config\\coins\\VRSC.json"

void ParseCoinConfig(padded_string& json, CoinConfig& cnfg);

// int valid(const char* s)
// {
//     unsigned char dec[32], d1[SHA256_DIGEST_LENGTH],
//     d2[SHA256_DIGEST_LENGTH];

//     coin_err = "";
//     if (!unbase58(s, dec)) return 0;

//     SHA256(SHA256(dec, 21, d1), SHA256_DIGEST_LENGTH, d2);

//     if (memcmp(dec + 21, d2, 4)) return 0;

//     return 1;
// }
int main(int argc, char** argv)
{
    Logger::Log(LogType::Info, LogField::Config, "Starting SickPool!");

    Logger::Log(LogType::Info, LogField::Config, "Static config:");
    Logger::Log(LogType::Info, LogField::Config, "Coin symbol: %s",
                COIN_SYMBOL);
    Logger::Log(LogType::Info, LogField::Config, "DB retention: %dms",
                DB_RETENTION);

    Logger::Log(LogType::Info, LogField::Config, "Loading dynamic config...");

    CoinConfig coinConfig;
    try
    {
        padded_string json = padded_string::load(CONFIG_PATH);
        ParseCoinConfig(json, coinConfig);

        Logger::Log(LogType::Info, LogField::Config, "Coin config loaded:");
        Logger::Log(LogType::Info, LogField::Config, "Pool address: %s",
                    coinConfig.pool_addr.c_str());
        Logger::Log(LogType::Info, LogField::Config, "Redis host: %s",
                    coinConfig.redis_host.c_str());
        Logger::Log(LogType::Info, LogField::Config, "PoW fee: %f%%",
                    coinConfig.pow_fee);
        Logger::Log(LogType::Info, LogField::Config, "PoS fee: %f%%",
                    coinConfig.pos_fee);

        StratumServer::coin_config = coinConfig;
        for (int i = 0; i < coinConfig.rpcs.size(); i++)
        {
            StratumServer::rpcs.push_back(new DaemonRpc(
                coinConfig.rpcs[i].host, coinConfig.rpcs[i].auth));
        }

        StratumServer stratumServer;
        stratumServer.StartListening();
    }
    catch (std::runtime_error e)
    {
        std::cerr << "START-UP ERROR: " << e.what() << "." << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_FAILURE; // should never finish!
}

void AssignJson(const char* name, std::string& obj, ondemand::document& doc)
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

void AssignJson(const char* name, ushort& obj, ondemand::document& doc)
{
    try
    {
        obj = doc[name].get_uint64();
    }
    catch (...)
    {
        throw std::runtime_error(std::string("Invalid or no \"") + name +
                                 "\" (uint) variable in config file");
    }
}

void AssignJson(const char* name, double& obj, ondemand::document& doc)
{
    try
    {
        obj = doc[name].get_double();
    }
    catch (...)
    {
        throw std::runtime_error(std::string("Invalid or no \"") + name +
                                 "\" (double) variable in config file");
    }
}

void ParseCoinConfig(padded_string& json, CoinConfig& cnfg)
{
    ondemand::parser confParser;
    ondemand::document configDoc = confParser.iterate(json);

    // AssignJson("name", cnfg->name, configDoc);
    // AssignJson("symbol", cnfg->symbol, configDoc);
    // AssignJson("algo", cnfg->algo, configDoc);
    AssignJson("stratum_port", cnfg.stratum_port, configDoc);
    AssignJson("pow_fee", cnfg.pow_fee, configDoc);
    AssignJson("pos_fee", cnfg.pos_fee, configDoc);
    AssignJson("default_diff", cnfg.default_diff, configDoc);
    AssignJson("target_shares_rate", cnfg.target_shares_rate, configDoc);
    AssignJson("pool_addr", cnfg.pool_addr, configDoc);
    AssignJson("redis_host", cnfg.redis_host, configDoc);

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

// make tests
// std::cout << std::hex << std::setfill('0') << std::setw(8)
//           << DiffToBits(32768) << std::endl;
// std::cout << BitsToDiff(DiffToBits(32768)) << std::endl;
// std::cout << BitsToDiff(DiffToBits(4096)) << std::endl;

// std::cout << BitsToDiff(DiffToTarget(1));

// const char* s[] = {"1Q1pE5vPGEEMqRcVRMbtBK842Y6Pzo6nK9",
//                    "1AGNa15ZQXAZUgFiqJ2i7Z2DPU2J6hW62i",
//                    "1Q1pE5vPGEEMqRcVRMbtBK842Y6Pzo6nJ9",
//                    "1AGNa15ZQXAZUgFiqJ2i7Z2DPU2J6hW62I", 0};
// int i;
// for (i = 0; s[i]; i++)
// {
//     int status = valid(s[i]);
//     printf("%s: %s\n", s[i], status ? "Ok" : "NO OK");
// }
// return 0;
//TODO: make difficulty adjustment based on average hashrate