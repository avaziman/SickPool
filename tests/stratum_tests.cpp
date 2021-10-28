#include <gtest/gtest.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "sock_addr.hpp"
#include "stratum/stratum_server.hpp"
#include "stratum_test.hpp"

TEST_F(StratumTest, Init)
{
    SockAddr addr("0.0.0.0:4444");
    stratumServ = new StratumServer(addr.ip, addr.port);
    ASSERT_NO_THROW();
}

TEST_F(StratumTest, StartListening)
{
    stratumServ->StartListening();
    ASSERT_NO_THROW();
}

TEST_F(StratumTest, Subscribe)
{
    char* buffer;
    GetReqBuffer(buffer, 9, "mining.subscribe", {"SicMiner/6.9"});
    send(sockfd, buffer, sizeof(buffer), 0);

    char recvBuffer[1024];
    recv(sockfd, recvBuffer, sizeof(recvBuffer), 0);
}