#include <iostream>

#include "crypto/uint256.h"
#include "daemon/daemon_rpc.hpp"
#include "sock_addr.hpp"
#include "stratum/stratum_server.hpp"

void DoWOrk(){

}
int main(int argc, char** argv)
{
    try
    {
        SockAddr rpcAddr("127.0.0.1:27486");
        DaemonRpc daemonRpc(
            rpcAddr.ip, rpcAddr.port,
            "c2lja3Bvb2w6MEU2d3ptTTE4VWVmZWl6SmxWWXRBZ21ENVdGZnJuVU"
            "ZTOUc0YUxTd2hsdw==");

        SockAddr stratumAddr("0.0.0.0:4444");
        StratumServer stratumServer(stratumAddr.ip, stratumAddr.port);

        daemonRpc.SendRequest(1, "getblock", {"1"});
    }
    catch (std::exception e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}