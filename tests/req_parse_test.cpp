#include <gtest/gtest.h>
#include <sys/socket.h>

#include "stratum_server.hpp"

TEST(ReqParse, SingleWholeReq)
{
    CoinConfig cfg{
        .pool_addr = "RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC",
        .pow_fee = 0.0,
        .pos_fee = 0.0,
        .default_diff = 1.0,
        .target_shares_rate = 1.0,
        .stratum_port = 9999,
        .control_port = 10000,
        .redis_port = 6379,
        .rpcs = {RpcConfig{"127.0.0.1:6004", "YWRtaW4xOnBhc3MxMjM="}},
        .hashrate_interval_seconds = 10,
        .effort_interval_seconds = 10,
        .average_hashrate_interval_seconds = 10,
        .hashrate_ttl_seconds = 10,
        .diff_adjust_seconds = 10};
    StratumServer stratum_server(cfg);

    std::thread t(&StratumServer::StartListening, &stratum_server);
    t.detach();

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in in{.sin_family = AF_INET,
                   .sin_port = htons(9999),
                   .sin_addr = {.s_addr = INADDR_ANY}};

    ASSERT_EQ(connect(sockfd, (const sockaddr*)&in, sizeof(in)), 0);

    send(sockfd, "")
}