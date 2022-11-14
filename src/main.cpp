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

StratumBase* stratum_bserver_ptr;

const Logger<config_field_str> logger;

void SigintHandler(int sig)
{
    logger.Log<LogType::Info>("Enter password:");
    // std::string pass;
    // std::cin >> pass;

    // if(pass == "1234"){
    logger.Log<LogType::Info>("Stopping stratum server...");

    stratum_bserver_ptr->Stop();
    // }
}

int main(int argc, char** argv)
{
    logger.Log<LogType::Info>("Starting SickPool!");
    logger.Log<LogType::Info>("Git commit hash: {}", GIT_COMMIT_HASH);

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

        logger.Log<LogType::Info>("Coin symbol: {}", coinConfig.symbol);

        if (coinConfig.symbol == "ZANO")
        {
            static constexpr StaticConf confs = ZanoStatic;
            logger.Log<LogType::Info>("Static config:");
            logger.Log<LogType::Info>("Diff1: {}", confs.DIFF1);
            logger.Log<LogType::Info>("Share multiplier: {}",
                                      pow2d(256) / confs.DIFF1);

            StratumServerCn<confs> stratum_server(std::move(coinConfig));
            stratum_bserver_ptr = dynamic_cast<StratumBase*>(&stratum_server);
            stratum_server.Listen();
        }
        else
        {
            logger.Log<LogType::Error>("Unknown coin symbol: {} exitting...",
                                       coinConfig.symbol);
        }
    }
    catch (const std::runtime_error& e)
    {
        logger.Log<LogType::Critical>("START-UP ERROR: {}.", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS; // graceful exit
}