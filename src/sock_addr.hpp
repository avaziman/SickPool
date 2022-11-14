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
    {
        std::string ip_str;
        std::string port_str;

        ip_str = sockAddrStr.substr(0, sockAddrStr.find(':'));
        port_str =
            sockAddrStr.substr(sockAddrStr.find(':') + 1, sockAddrStr.size());

        ip = inet_addr(ip_str.c_str());
        port = htons(static_cast<uint16_t>(std::stoul(port_str)));
    }
    unsigned short port;
    in_addr_t ip;
};
#endif