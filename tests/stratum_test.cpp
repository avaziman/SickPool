// #include <gtest/gtest.h>

// #ifdef _WIN32
// #include <winsock2.h>
// #endif

// #include "sock_addr.hpp"
// #include "stratum/stratum_server.hpp"
// #include "stratum_test.hpp"

// StratumServer* stratumServ;
// SOCKET sockfd;

// TEST_F(StratumTest, Init)
// {
//     stratumServ = new StratumServer(htonl(INADDR_ANY), htons(4444));
//     ASSERT_NO_THROW();
// }

// TEST_F(StratumTest, StartListening)
// {
//     stratumServ->StartListening();
//     ASSERT_NO_THROW();
// }

// TEST_F(StratumTest, Connect)
// {
//     sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

//     ASSERT_NE(sockfd, INVALID_SOCKET);

//     sockaddr_in addr;
//     addr.sin_family = AF_INET;
//     addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
//     addr.sin_port = htons(4444);

//     int res = connect(sockfd, (const sockaddr*)&addr, sizeof(addr));
//     int err = WSAGetLastError();
//     ASSERT_EQ(res, 0);
// }

// TEST_F(StratumTest, Subscribe)
// {
//     char buffer[2048];
//     GetReqBuffer(buffer, 3, "mining.subscribe", {"SicMiner/6.9"});
//     int res = send(sockfd, buffer, sizeof(buffer), 0);

//     char recvBuffer[1024];
//     recv(sockfd, recvBuffer, sizeof(recvBuffer), 0);
// }