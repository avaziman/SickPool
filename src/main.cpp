#ifdef _WIN32
#include <ws2tcpip.h>  // inet_ntop, inet_pton
#endif

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "crypto/hash_wrapper.hpp"
#include "crypto/verus_transaction.hpp"
#include "crypto/verushash/verus_hash.h"
#include "daemon/daemon_rpc.hpp"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/rapidjson.h"
#include "sock_addr.hpp"
#include "stratum/stratum_server.hpp"

#define CONFIG_PATH_VRSC \
    "C:\\Projects\\Pool\\pool-server\\config\\coins\\VRSC.json"
// #define CONFIG_PATH_VRSC                                                      \
//     "/media/sickguy/B0A8A4E4A8A4A9F4/Projects/Pool/pool-server/config/coins/" \
//     "VRSC.json"

bool ParseCoinConfig(CoinConfig* cnfg, const char* path);

int main(int argc, char** argv)
{
#ifdef _WIN32
    WSADATA wsdata;
    if (WSAStartup(MAKEWORD(2, 2), &wsdata) != 0) exit(-1);
#endif
    CoinConfig coinConfig;
    if (!ParseCoinConfig(&coinConfig, CONFIG_PATH_VRSC))
    {
        std::cerr << "START UP ERROR: "
                  << "failed to parse coinconfig";
        return EXIT_FAILURE;
    }

    try
    {
        StratumServer stratumServer(coinConfig);
        stratumServer.StartListening();
    }
    catch (std::exception e)
    {
        std::cerr << "START UP ERROR: " << e.what() << std::endl;

#ifdef _WIN32
        WSACleanup();
#endif

        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

bool ParseCoinConfig(CoinConfig* cnfg, const char* path)
{
    try
    {
        std::ifstream configFile(CONFIG_PATH_VRSC);
        IStreamWrapper is(configFile);

        rapidjson::Document configDoc(rapidjson::kObjectType);
        configDoc.ParseStream(is);

        cnfg->name = configDoc["name"].GetString();
        cnfg->symbol = configDoc["symbol"].GetString();
        cnfg->algo = configDoc["algo"].GetString();
        cnfg->stratum_port = configDoc["stratum_port"].GetUint();
        cnfg->pow_fee = configDoc["pow_fee"].GetFloat();
        cnfg->pos_fee = configDoc["pos_fee"].GetFloat();
        cnfg->default_diff = configDoc["default_difficulty"].GetDouble();
        cnfg->pool_addr = configDoc["pool_addr"].GetString();
        auto rpcs = configDoc["rpcs"].GetArray();

        for (int i = 0; i < rpcs.Size(); i++)
        {
            cnfg->rpcs[i].host = rpcs[i]["host"].GetString();
            cnfg->rpcs[i].auth = rpcs[i]["auth"].GetString();
        }

        configFile.close();
        return true;
    }
    catch(...)
    {
        return false;
    }
}