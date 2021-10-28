#include <gtest/gtest.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <vector>
using namespace rapidjson;

#include "stratum/stratum_server.hpp"

class StratumTest : public ::testing::Test
{
   protected:
    StratumServer* stratumServ;
    SOCKET sockfd;

    static void SetUpTestSuite()
    {
        WSADATA wsdata;
        if (WSAStartup(MAKEWORD(2, 2), &wsdata) != 0) FAIL();
    }

    static void TearDownTestSuite()
    {
        WSACleanup();
    }

    virtual void SetUp() override
    {
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        ASSERT_NE(sockfd, INVALID_SOCKET);
    }

    virtual void TearDown() override { closesocket(sockfd); }

    void GetReqBuffer(char* buffer, int id, const char* method,
                      std::vector<const char*> params)
    {
        Document d(kObjectType);
        d.AddMember("id", id, d.GetAllocator());
        d.AddMember("method", StringRef(method), d.GetAllocator());

        Value paramsArr(kArrayType);
        for (const char* param : params)
            paramsArr.PushBack(StringRef(param), d.GetAllocator());

        d.AddMember("params", paramsArr, d.GetAllocator());

        StringBuffer sb;
        Writer<StringBuffer> writer(sb);

        d.Accept(writer);

        sprintf(buffer, sb.GetString());
    }
};

// only used in tests but still has to be tested
TEST_F(StratumTest, GetReqBuffer)
{
    char buffer[1024];
    GetReqBuffer(buffer, 12, "mymethod", {"param1", "param2"});

    char control_buffer[1024];
    sprintf(control_buffer,
            "{\"id\":12,\"method\":\"mymethod\",\"params\":[\"param1\","
            "\"param2\"]}");

    ASSERT_FALSE(strcmp(buffer, control_buffer));
}