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
    SockAddr(std::string sockAddrStr)
    {
        std::string ip_str, port_str;

        ip_str = sockAddrStr.substr(0, sockAddrStr.find(':'));
        port_str =
            sockAddrStr.substr(sockAddrStr.find(':') + 1, sockAddrStr.size());

        ip = inet_addr(ip_str.c_str());
        port = htons(std::stoi(port_str));
    }
    unsigned short port;
    unsigned long ip;
};
#endif