#ifndef CONNECTION_HPP_
#define CONNECTION_HPP_

#include <arpa/inet.h>

#include <list>
#include <memory>

#include "static_config.hpp"

template <typename T>
struct Connection {
    Connection(int s, in_addr ip_addr) : sock(s) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));
        ip = std::string(ip_str);
    }
    int sock;
    size_t req_pos = 0;
    char req_buff[REQ_BUFF_SIZE];
    std::shared_ptr<T> ptr;
    std::list<std::shared_ptr<Connection>>::iterator it;
    std::string ip;
};

#endif