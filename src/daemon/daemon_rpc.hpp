#ifndef DAEMON_API_HPP_
#define DAEMON_API_HPP_

#include <arpa/inet.h>  //inet_ntop
#include <netinet/in.h>
#include <simdjson.h>
#include <sys/socket.h>
#include <unistd.h>  //close

#include <any>
#include <chrono>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "../sock_addr.hpp"

#define HTTP_HEADER_SIZE (1024 * 4)

class DaemonRpc
{
   public:
    DaemonRpc(const std::string& hostHeader,
                         const std::string& authHeader)
        : host_header(hostHeader), auth_header(authHeader)
    {
        SockAddr sock_addr(hostHeader);

        rpc_addr.sin_family = AF_INET;
        rpc_addr.sin_addr.s_addr = sock_addr.ip;
        rpc_addr.sin_port = sock_addr.port;
    }

    template <typename... T>
    int SendRequest(std::string& result, int id, const char* method,
                    T... params) /*const*/
    {
        size_t bodySize, sendSize, recvSize;
        int errCode, resCode;
        int sent, contentLength, contentReceived, headerLength;

        // initialize the socket
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (sockfd <= 0 /*|| sockfd == INVALID_SOCKET*/)
        {
            return errno;
        }

        if (connect(sockfd, (const sockaddr*)&rpc_addr, sizeof(rpc_addr)) < 0)
        {
            return errno;
        }

        std::string params_json;

        if constexpr (sizeof...(params) != 0)
        {
            auto params_vec = std::vector{params...};
            for (int i = 0; i < params_vec.size(); i++)
            {
                std::any& param = params_vec[i];

                if (param.type() == typeid(std::string_view))
                {
                    auto param_sv = std::any_cast<std::string_view>(param);
                    params_json.append("\"");
                    params_json.append(param_sv);
                    params_json.append("\"");
                }
                else if (param.type() == typeid(int))
                {
                    // std::any_cast<std::string_view>(param)
                    std::string str = std::to_string(std::any_cast<int>(param));
                    params_json.append(str);
                }
                else
                {
                    // static_assert(false, "Invalid rpc parameter, may only be
                    // string_view or int");
                }

                if (i != params_vec.size() - 1)
                {
                    params_json.append(",");
                }
            }
        }

        // generate the http request

        std::size_t bodyLen = params_json.size() + 64;
        auto body = std::make_unique<char[]>(bodyLen);

        bodySize = snprintf(body.get(), bodyLen,
                            "{\"id\":%d,\"method\":\"%s\",\"params\":[%s]}", id,
                            method, params_json.c_str());

        const std::size_t sendBufferLen = bodySize + 256;
        auto sendBuffer = std::make_unique<char[]>(bodySize + 256);
        sendSize = snprintf(sendBuffer.get(), sendBufferLen,
                            "POST / HTTP/1.1\r\n"
                            "Host: %s\r\n"
                            "Authorization: Basic %s\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: %ld\r\n\r\n"
                            "%s\r\n\r\n",
                            host_header.c_str(), auth_header.c_str(), bodySize,
                            body.get());

        sent = send(sockfd, (void*)sendBuffer.get(), sendSize, 0);

        if (sent < 0) return -1;

        char headerBuff[HTTP_HEADER_SIZE];

        int headerRecv = 0;
        char* endOfHeader = 0;
        // receive http header (and potentially part or the whole body)
        do
        {
            std::size_t recvRes = recv(sockfd, headerBuff + headerRecv,
                                       HTTP_HEADER_SIZE - headerRecv - 1, 0);
            if (recvRes <= 0)
            {
                return errno;
            }
            headerRecv += recvRes;
            // for the response end check, gets overriden
            headerBuff[headerRecv - 1] = '\0';
        } while ((endOfHeader = std::strstr(headerBuff, "\r\n\r\n")) ==
                 nullptr);

        endOfHeader += 4;

        resCode = std::atoi(headerBuff + std::strlen("HTTP/1.1 "));

        contentLength = std::atoi(std::strstr(headerBuff, "Content-Length: ") +
                                  sizeof("Content-Length: ") - 1);
        contentReceived = std::strlen(endOfHeader);
        // contentReceived = headerRecv - (endOfHeader - headerBuff);

        // simd json parser requires some extra bytes
        result.reserve(contentLength + simdjson::SIMDJSON_PADDING);
        result.resize(contentLength - 1);

        // sometimes we will get 404 message after the json
        // (its length not included in content-length)
        contentReceived = std::min(contentReceived, contentLength);
        memcpy(result.data(), endOfHeader, contentReceived);

        // receive http body if it wasn't already
        while (contentReceived < contentLength)
        {
            std::size_t recvRes = recv(sockfd, result.data() + contentReceived,
                                       contentLength - contentReceived, 0);
            if (recvRes <= 0)
            {
                return errno;
            }
            contentReceived += recvRes;
        }

        // result[contentReceived - 1] = '\0';

        close(sockfd);
        return resCode;
    }

   private:
    int sockfd;
    sockaddr_in rpc_addr;
    std::string host_header;
    std::string auth_header;
};
#endif