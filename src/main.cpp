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
#include "stratum_server_zec.hpp"
#include "stratum_server_cn.hpp"

StratumBase* stratum_bserver_ptr;

const Logger logger{config_field_str};
using enum LogType;

void SigintHandler(int sig)
{
    logger.Log<Info>("Received signal {}.", sig);

#ifdef DEBUG
    std::string pass;
    std::cin >> pass;

    if (pass != "1234"){
        logger.Log<Info>("Bad password, NOT shutting down...");

        return;
    } 
#endif

    logger.Log<Info>("Stopping stratum server...");

    stratum_bserver_ptr->Stop();
}

void PrintStaticStats(StaticConf confs){
        logger.Log<Info>("Static config:");
        logger.Log<Info>("Diff1: {}", confs.DIFF1);
        logger.Log<Info>("Share multiplier: {}", pow2d(256) / confs.DIFF1);
}

int main(int argc, char** argv)
{
    logger.Log<Info>("Starting SickPool!");
    logger.Log<Info>("Git commit hash: {}", GIT_COMMIT_HASH);
    
    logger.Log<Info>("Loading dynamic config...");

    if (signal(SIGINT, SigintHandler) == SIG_ERR)
    {
        logger.Log<Error>("Failed to register SIGINT...");
    }

    CoinConfig coinConfig;
    try
    {
        if (argc < 2 || !std::ifstream(argv[1]).good())
        {
            throw std::invalid_argument("Bad config file specified");
        }

        simdjson::padded_string json = simdjson::padded_string::load(argv[1]);
        ParseCoinConfig(json, coinConfig, logger);

        logger.Log<Info>("Coin symbol: {}", coinConfig.symbol);

        if (coinConfig.symbol == "ZANO")
        {
            static constexpr StaticConf confs = ZanoStatic;
            PrintStaticStats(confs);

            StratumServerCn<confs> stratum_server(std::move(coinConfig));
            stratum_bserver_ptr = dynamic_cast<StratumBase*>(&stratum_server);
            stratum_server.Listen();
        }
        else if (coinConfig.symbol == "VRSC"){
            static constexpr StaticConf confs = VrscStatic;
            PrintStaticStats(confs);

            StratumServerZec<confs> stratum_server(std::move(coinConfig));
            stratum_bserver_ptr = dynamic_cast<StratumBase*>(&stratum_server);
            stratum_server.Listen();
        }else 
        {
            logger.Log<Error>("Unknown coin symbol: {} exitting...",
                              coinConfig.symbol);
        }
    }
    catch (const std::invalid_argument& e)
    {
        logger.Log<Critical>("START-UP ERROR: {}.", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;  // graceful exit
}

