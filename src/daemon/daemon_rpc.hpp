#ifndef DAEMON_API_HPP_
#define DAEMON_API_HPP_

#include <arpa/inet.h>  //inet_ntop
#include <fmt/core.h>
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
#include <type_traits>
#include <variant>
#include <vector>

#include "../sock_addr.hpp"
#include "jsonify.hpp"

#define HTTP_HEADER_SIZE (1024 * 16)

class DaemonRpc
{
   public:
    DaemonRpc(const std::string& hostHeader, const std::string& authHeader)
        : host_header(hostHeader), auth_header(authHeader)
    {
        SockAddr sock_addr(hostHeader);

        rpc_addr.sin_family = AF_INET;
        rpc_addr.sin_addr.s_addr = sock_addr.ip;
        rpc_addr.sin_port = sock_addr.port;
    }

    static std::string ToJsonStr(int arg)
    {
        std::string str = std::to_string(arg);
        return str;
    }

    static std::string ToJsonStr(std::string_view arg)
    {
        return fmt::format("\"{}\"", arg);
    }

    template <typename... KeyVal>
    static std::string ToJsonObj(KeyVal... obj)
    {
        std::string res = "{";
        auto append = [&res](std::pair<std::string_view, auto> x)
        {
            res += ToJsonStr(x.first, x.second);
            res += ",";
        };

        (append(obj), ...);
        res[res.size() - 1] = '}';
        return res;
    }

    template <typename T>
    static std::string ToJsonStr(std::string_view key, T val)
    {
        return fmt::format("\"{}\":{}", key, ToJsonStr(val));
    }

    template <typename... T>
    static std::string GetArrayStr(T&&... args)
    {
        std::string params_json = "[";

        auto append = [&params_json](auto s)
        {
            params_json += ToJsonStr(s);
            params_json.append(",");
        };

        (append(args), ...);

        params_json[params_json.size() - 1] = ']';
        return params_json;
    }

    int SendRequest(std::string& result, int id, std::string_view method,
                    std::string_view params_json,
                    std::string_view type = "POST /") /*const*/
    {
        int res_code;
        size_t body_size;
        size_t send_size;
        ssize_t sent;
        size_t content_length;
        size_t content_received;

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

        // generate the http request

        std::size_t bodyLen = params_json.size() + 128;
        auto body = std::make_unique<char[]>(bodyLen);

        body_size = fmt::format_to_n(body.get(), bodyLen,
                                     "{{\"jsonrpc\":\"2.0\",\"id\":{},"
                                     "\"method\":\"{}\",\"params\":{}}}",
                                     id, method, params_json)
                        .size;

        const std::size_t send_buffer_len = body_size + 256;
        auto send_buffer = std::make_unique<char[]>(body_size + 256);
        send_size = fmt::format_to_n(send_buffer.get(), send_buffer_len,
                                     "{} HTTP/1.1\r\n"
                                     "Host: {}\r\n"
                                     "Authorization: Basic {}\r\n"
                                     "Content-Type: application/json\r\n"
                                     "Content-Length: {}\r\n\r\n"
                                     "{}\r\n\r\n",
                                     type, host_header, auth_header, body_size,
                                     std::string_view(body.get(), body_size))
                        .size;

        sent = send(sockfd, (void*)send_buffer.get(), send_size, 0);

        if (sent < 0) return -1;

        char headerBuff[HTTP_HEADER_SIZE];

        size_t header_recv = 0;
        const char* end_of_header = nullptr;
        // receive http header (and potentially part or the whole body)
        do
        {
            ssize_t recvRes = recv(sockfd, headerBuff + header_recv,
                                       HTTP_HEADER_SIZE - header_recv, 0);
            if (recvRes <= 0)
            {
                return errno;
            }
            header_recv += recvRes;
            // for the response end check, gets overriden
            headerBuff[header_recv] = '\0';
        } while ((end_of_header = std::strstr(headerBuff, "\r\n\r\n")) ==
                 nullptr);

        end_of_header += 4;

        res_code = std::atoi(headerBuff + sizeof("HTTP/1.1 ") - 1);

        // doens't include error message
        content_length = std::atoi(std::strstr(headerBuff, "Content-Length: ") +
                                   sizeof("Content-Length: ") - 1);
        content_received = header_recv - (end_of_header - headerBuff);

        if (res_code != 200)
        {
            // if there was an error return the header instead the body as there
            // is none
            result.resize(header_recv);
            memcpy(result.data(), headerBuff, header_recv);
            close(sockfd);
            return res_code;
        }

        // simd json parser requires some extra bytes
        result.reserve(content_length + simdjson::SIMDJSON_PADDING);
        result.resize(content_length);

        // sometimes we will get 404 message after the json
        // (its length not included in content-length)
        content_received = std::min(content_received, content_length);
        memcpy(result.data(), end_of_header, content_received);

        // receive http body if it wasn't already
        while (content_received < content_length)
        {
            std::size_t recvRes = recv(sockfd, result.data() + content_received,
                                       content_length - content_received, 0);
            if (recvRes <= 0)
            {
                return res_code;
            }
            content_received += recvRes;
        }

        close(sockfd);
        return res_code;
    }

   private:
    int sockfd;
    sockaddr_in rpc_addr;
    std::string host_header;
    std::string auth_header;
};
#endif