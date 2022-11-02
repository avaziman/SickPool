#include <signal.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "config_parser.hpp"
#include "crypto/base58.h"
#include "daemon/daemon_rpc.hpp"
#include "logger.hpp"
#include "sock_addr.hpp"
#include "static_config.hpp"
#include "stratum/stratum_server.hpp"
// #define CONFIG_PATH                                                   \
//     "//home/sickguy/Documents/Projects/SickPool/server/config/coins/" \
//     "VRSC.json"
// #define CONFIG_PATH_VRSC \
//     "C:\\projects\\pool\\pool-server\\config\\coins\\VRSC.json"

// void ParseCoinConfig(const simdjson::padded_string& json, CoinConfig& cnfg);

stratum_server_t* stratum_server_ptr;

Logger<config_field_str> logger;

void SigintHandler(int sig)
{
    logger.Log<LogType::Info>("Enter password:");
    // std::string pass;
    // std::cin >> pass;

    // if(pass == "1234"){
    logger.Log<LogType::Info>("Stopping stratum server...");

    stratum_server_ptr->Stop();
    // }
}

int main(int argc, char** argv)
{
    // std::cout << std::hex
    //           <<
    //           UintToArith256(uint256S("00000000000184c09e98da047ab3260fca551"
    //                                      "c8c476551b63140d99b634aea2d"))
    //                  .GetCompact();
    // std::cout << "Block Submission Size: " << sizeof(BlockSubmission);
    logger.Log<LogType::Info>("Starting SickPool!");
    logger.Log<LogType::Info>("Git commit hash: {}", GIT_COMMIT_HASH);

    logger.Log<LogType::Info>("Static config:");
    logger.Log<LogType::Info>("Coin symbol: {}", COIN_SYMBOL);

    logger.Log<LogType::Info>("Loading dynamic config...");

    if (signal(SIGINT, SigintHandler) == SIG_ERR)
    {
        logger.Log<LogType::Error>("Failed to register SIGINT...");
    }

    CoinConfig coinConfig;
    try
    {
        if (argc < 2 || !std::ifstream(argv[1]).good())
        {
            throw std::runtime_error("Bad config file specified");
        }

        simdjson::padded_string json = simdjson::padded_string::load(argv[1]);
        ParseCoinConfig(json, coinConfig, logger);

        stratum_server_t stratum_server(coinConfig);
        stratum_server_ptr = &stratum_server;
        stratum_server.Listen();
    }
    catch (const std::runtime_error& e)
    {
        logger.Log<LogType::Critical>("START-UP ERROR: {}.", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_FAILURE;  // should never finish!
}