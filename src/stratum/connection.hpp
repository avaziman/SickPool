#ifndef CONNECTION_HPP_
#define CONNECTION_HPP_

#include <arpa/inet.h>

#include <string>
#include <list>
#include <memory>

#include "static_config.hpp"

template <typename T>
struct Connection
{
    public: 
    explicit Connection(const int sfd, const in_addr& addr, const int tfd = 0) : sockfd(sfd), timerfd(tfd), ip(ip_str, sizeof(ip_str))
    {
        inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
    }
    
    const int sockfd;
    const int timerfd;
    const std::string_view ip;
    int expiration_count = 0;

    size_t req_pos = 0;
    char req_buff[REQ_BUFF_SIZE];
    std::shared_ptr<T> ptr;
    std::list<std::shared_ptr<Connection<T>>>::iterator it;

    // private:
     char ip_str[INET_ADDRSTRLEN] = {0};
};

#endif