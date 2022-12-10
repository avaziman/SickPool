#ifndef SOCKADDR_HPP_
#define SOCKADDR_HPP_

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <string>

class SockAddr
{
   public:
    explicit SockAddr(std::string_view sockAddrStr)
        : ip_str(sockAddrStr.substr(0, sockAddrStr.find(':')))
    {
        std::string port_str;

        port_str =
            sockAddrStr.substr(sockAddrStr.find(':') + 1, sockAddrStr.size());

        ip = inet_addr(ip_str.c_str());
        port_original = static_cast<uint16_t>(std::stoul(port_str));

        port = htons(port_original);
    }

    const std::string ip_str;
    uint16_t port_original;
    uint16_t port;
    in_addr_t ip;
};
#endif