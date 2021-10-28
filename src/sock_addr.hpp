#ifndef SOCKADDR_HPP_
#define SOCKADDR_HPP_

#include <ws2tcpip.h>

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

        inet_pton(AF_INET, ip_str.c_str(), &ip);
        port = htons(std::stoi(port_str));
    }
    unsigned short port;
    unsigned long ip;
};
#endif