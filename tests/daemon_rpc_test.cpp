#include "../src/daemon/daemon_rpc.hpp"

#include <gtest/gtest.h>
#include <rapidjson/rapidjson.h>

#include "../src/sock_addr.hpp"

class DaemonRpcTest : public ::testing::Test
{
   public:
    static void SetUpTestSuite()
    {
#ifdef _WIN32
        WSADATA wsdata;
        if (WSAStartup(MAKEWORD(2, 2), &wsdata) != 0) exit(-1);
#endif
    }

   protected:
    void SetUp()
    {
        this->rpc = new DaemonRpc(
            "127.0.0.1:27484",
            "c2lja3Bvb2w6MEU2d3ptTTE4VWVmZWl6SmxWWXRBZ21ENVdGZnJuVUZTOUc0"
            "YUxTd2hsdw");
    }
    DaemonRpc* rpc;
};

TEST_F(DaemonRpcTest, EmtpyMethod)
{
    char* res = this->rpc->SendRequest(1, "method");
    ASSERT_NE(res, nullptr);
}

TEST_F(DaemonRpcTest, GetBlock)
{
    char* res = this->rpc->SendRequest(1, "getblock", "0");
    ASSERT_NE(res, nullptr);
}

TEST_F(DaemonRpcTest, Send2MbReq)
{
    // std::string param = std::string(2 * 1000 * 1000, '0');

    // char* res = this->rpc->SendRequest(1, "method", param);
    // ASSERT_NE(res, nullptr);
}