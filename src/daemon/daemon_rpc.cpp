#include "daemon_rpc.hpp"

#include <iostream>
DaemonRpc::DaemonRpc(u_long rpcIp, u_short rpcPort, const char* authHeader)
    : auth_header(authHeader)
{
    Init();
    char ip_str[16];
    inet_ntop(AF_INET, &rpcIp, ip_str, sizeof(ip_str));
    host_header = std::string(ip_str) + ":" + std::to_string(ntohs(rpcPort));

    rpc_addr.sin_family = AF_INET;
    rpc_addr.sin_addr.S_un.S_addr = rpcIp;
    rpc_addr.sin_port = rpcPort;
}

void DaemonRpc::Init()
{
#ifdef _WIN32
    WSADATA wsdata;
    if (WSAStartup(MAKEWORD(2, 2), &wsdata) != 0) exit(-1);
#endif
    // create socket file descriptor (identifier)
    sockfd =
        socket(AF_INET,      // adress family: ipv4
               SOCK_STREAM,  // socket type: socket stream, reliable byte
                             // connection-based byte stream
               IPPROTO_TCP   // protocol: transmission control protocol (tcp)
        );

    if (sockfd <= 0) exit(-1);//change ot retunr
}

DaemonRpc::~DaemonRpc()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

void DaemonRpc::SendRequest(int id, const char* method,
                            std::vector<const char*> params)
{
    if (connect(sockfd, (const sockaddr*)&rpc_addr, sizeof(rpc_addr)) < 0)
    {
        exit(-1);
    }

    Document reqBody(kObjectType);

    reqBody.AddMember("method", StringRef(method, sizeof(method)), reqBody.GetAllocator());

    Value paramsArr(kArrayType);
    for (const char* param : params) {
        paramsArr.PushBack(StringRef(param), reqBody.GetAllocator());
    }
    reqBody.AddMember("params", paramsArr, reqBody.GetAllocator());
    reqBody.AddMember("id", id, reqBody.GetAllocator());

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    reqBody.Accept(writer);

    char sendBuffer[1024];
    sprintf(sendBuffer,
            "POST / HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Authorization: Basic %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n\r\n"
            "%s\r\n",
            host_header.c_str(), auth_header, buffer.GetLength(),
            buffer.GetString());

    int sent = send(sockfd, sendBuffer, sizeof(sendBuffer), 0);
    if (sent < 0)
    {
        exit(-1);
    }

    char resBuffer[1024];
    int recvRes = recv(sockfd, resBuffer, sizeof(resBuffer), 0);

    if (recvRes < 0) exit(-1);

    // closesocket(sockfd);
    std::cout << resBuffer << std::endl;
    std::cout << sent << std::endl;
}