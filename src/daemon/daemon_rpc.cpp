#include "daemon_rpc.hpp"

#include <chrono>
#include <iostream>

#ifdef _WIN32
#define SOCK_ERR (WSAGetLastError())
#else
#define SOCK_ERR (errno)
#endif

#define LOG(x, y) (std::cout << x << ": " << y << std::endl)

using namespace std;

DaemonRpc::DaemonRpc(u_long rpcIp, u_short rpcPort, std::string authHeader)
    : auth_header(authHeader)
{
    char ip_str[16];
    inet_ntop(AF_INET, &rpcIp, ip_str, sizeof(ip_str));
    host_header = std::string(ip_str) + ":" + std::to_string(ntohs(rpcPort));

    rpc_addr.sin_family = AF_INET;
    rpc_addr.sin_addr.s_addr = rpcIp;
    rpc_addr.sin_port = rpcPort;
}

char* DaemonRpc::SendRequest(int id, const char* method, const char* param)
{
    char /**body, *sendBuffer,*/* recvBuffer;
    int bodySize, sendSize, recvSize, errCode, resCode;
    int sent, contentLength, contentReceived;

    // generate the http request

    // body = new char[std::strlen(param) + 64];

    // bodySize = sprintf(body,
    // "{\"id\":%d,\"method\":\"%s\",\"params\":[\"%s\"]}",
    //                    id, method, param);

    std::ostringstream ss;
    ss << "{\"id\":" << id << ",\"method\":" << method << ",\"params\":[\""
       << param << "\"])";
    auto s = ss.str();
    bodySize = s.size();
    const char* body = s.c_str();

    // sendBuffer = new char[bodySize + 256];
    // sendSize =
    //     sprintf(sendBuffer,
    //             "POST / HTTP/1.1\r\n"
    //             "Host: %s\r\n"
    //             "Authorization: Basic %s\r\n"
    //             "Content-Type: application/json\r\n"
    //             "Content-Length: %d\r\n\r\n"
    //             "%s\r\n\r\n",
    //             host_header.c_str(), auth_header.c_str(), bodySize, body);

    std::ostringstream sss;
    sss << "POST / HTTP/1.1\r\n";
    sss << "Host: " << host_header << "\r\n";
    sss << "Authorization: Basic " << auth_header << "\r\n";
    sss << "Content-Type: application/json\r\n";
    sss << "Content-Length: " << bodySize << "\r\n\r\n";
    sss << body << "\r\n\r\n";
    auto ff = sss.str();
    const char* sendBuffer = ff.c_str();
    sendSize = ff.size();

    // initialize the socket
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sockfd <= 0 /*|| sockfd == INVALID_SOCKET*/)
    {
        errCode = SOCK_ERR;

        std::cout << "failed to create socket, error code: " << errCode
                  << std::endl;
        return nullptr;
    }

    if (connect(sockfd, (const sockaddr*)&rpc_addr, sizeof(rpc_addr)) < 0)
    {
        errCode = SOCK_ERR;

        std::cerr << "failed to connect to rpc socket, error code: " << errCode
                  << std::endl;
        return nullptr;
    }

    sent = send(sockfd, sendBuffer, sendSize, 0);

    std::cout << "sent " << sent << " out of " << sendSize << std::endl;
    // TODO: add while here (if it ever gives a problem)
    if (sent < 0) return nullptr;

    recvBuffer = new char[HEADER_SIZE];

    int totalRecv = 0;
    char* endOfHeader = 0;
    // receive http header (and potentially part or the whole body)
    do
    {
        int recvRes = recv(sockfd, recvBuffer, HEADER_SIZE, 0);

        if (recvRes == 0)
        {
            std::cerr << "rpc socket disconnected." << std::endl;
            return nullptr;
        }
        else if (recvRes < 0)
        {
            errCode = SOCK_ERR;

            std::cerr << "rpc socket receive error code: " << errCode
                      << std::endl;
            return nullptr;
        }
        totalRecv += recvRes;
    } while ((endOfHeader = std::strstr(recvBuffer, "\r\n\r\n")) == NULL);

    endOfHeader += 4;
    recvBuffer[totalRecv] = '\0';

    resCode = std::atoi(recvBuffer + std::strlen("HTTP/1.1 "));
    contentLength = std::atoi(std::strstr(recvBuffer, "Content-Length: ") +
                              std::strlen("Content-Length: "));
    contentReceived = std::strlen(endOfHeader);

    // std::cout << "HTTP CODE: " << resCode << std::endl;
    // std::cout << "CONTENT LENGTH: " << contentLength << std::endl;
    // std::cout << "CONTENT RECEIVED: " << contentReceived << std::endl;
    // std::cout << "TOTAL RECEIVED: " << totalRecv << std::endl;

    char* resBuffer = new char[contentLength];
    std::strcpy(resBuffer, endOfHeader);

    // receive http body if it wasn't already
    while (contentReceived < contentLength)
    {
        int recvRes = recv(sockfd, resBuffer + contentReceived,
                           contentLength - contentReceived, 0);
        if (recvRes == 0)
        {
            std::cerr << "rpc socket disconnected." << std::endl;
            return nullptr;
        }
        else if (recvRes < 0)
        {
            int errCode = SOCK_ERR;

            std::cerr << "rpc socket receive error code: " << errCode
                      << std::endl;
            return nullptr;
        }
        contentReceived += recvRes;
    }
    resBuffer[contentReceived] = '\0';

    // delete[] body;
    delete[] recvBuffer;
    // delete[] sendBuffer;
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
    return resBuffer;
    // return "o";
}