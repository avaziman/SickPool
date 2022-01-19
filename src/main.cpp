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

void ParseCoinConfig(CoinConfig* cnfg, const char* path);

int main(int argc, char** argv)
{
#ifdef _WIN32
    WSADATA wsdata;
    if (WSAStartup(MAKEWORD(2, 2), &wsdata) != 0) exit(-1);
#endif
    CoinConfig coinConfig;
    ParseCoinConfig(&coinConfig, CONFIG_PATH_VRSC);

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

void ParseCoinConfig(CoinConfig* cnfg, const char* path)
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
    auto rpcs = configDoc["rpcs"].GetArray();

    for (int i = 0; i < rpcs.Size(); i++)
    {
        cnfg->rpcs[i].host = rpcs[i]["host"].GetString();
        cnfg->rpcs[i].auth = rpcs[i]["auth"].GetString();
    }

    configFile.close();
}

// SockAddr so("127.0.0.1:27486");
// DaemonRpc dr(so.ip, so.port,
//              "c2lja3Bvb2w6MEU2d3ptTTE4VWVmZWl6SmxWWXRBZ21ENVdGZnJuVUZTOUc0"
//              "YUxTd2hsdw==");
// char* buffer;
// try
// {
//     buffer = dr.SendRequest(1, "getblocktemplate", {});
// }
// catch (const std::exception& e)
// {
//     std::cerr << "BLOCK UPDATE RPC ERROR: " << e.what() << std::endl;
// }
// std::cout << "BUFFER: " << buffer << std::endl;