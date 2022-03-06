// #include <gtest/gtest.h>

// #include <string>
// #include "sock_addr.hpp"
// #include "stratum/stratum_server.hpp"
// #include "stratum_test.hpp"
// #include <simdjson.h>
// #include "../src/coin_config.hpp"

// #define PORT 4444

// StratumServer* stratumServ;
// int sockfd;
// using namespace simdjson;

// TEST_F(StratumTest, Init)
// {
//     CoinConfig coinConfig;
//     coinConfig.algo = "verushash";
//     coinConfig.stratum_port = 4444;
//     coinConfig.default_diff = 0;
//     coinConfig.redis_host = "127.0.0.1:6379";

//     stratumServ = new StratumServer(coinConfig);
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

//     ASSERT_NE(sockfd, -1);

//     sockaddr_in addr;
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
//     addr.sin_port = htons(PORT);

//     int res = connect(sockfd, (const sockaddr*)&addr, sizeof(addr));
//     ASSERT_EQ(res, 0);
// }

// TEST_F(StratumTest, Subscribe)
// {
//     // session id, host, host port = null
//     char buffer[256] = "{\"id\":1,\"method\":\"method.subsribe\",params:[\"miner1.0\",null, null, null]}\n";

//     int res = send(sockfd, buffer, sizeof(buffer), 0);

//     char recvBuffer[1024];
//     recv(sockfd, recvBuffer, sizeof(recvBuffer), 0);

//     ondemand::parser parser;
//     // char[] expected = "{"id": 1, "result": ["SESSION_ID", "NONCE_1"], "error": null}\n"
// }