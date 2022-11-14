#ifndef MANAGE_SERVER_HPP_
#define MANAGE_SERVER_HPP_

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "static_config.hpp"

enum class ControlCommands
{
    NONE = 0,
    BLOCK_NOTIFY = 1,
    // WALLET_NOTFIY = 2,
};

// struct WalletNotify
// {
//     char block_hash[HASH_SIZE];
//     char txid[HASH_SIZE];
//     char wallet_address[ADDRESS_LEN];
// };

class ControlServer
{
   public:
    void Start(unsigned short port)
    {
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (int optval = 1; setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                                       &optval, sizeof(optval)) != 0)
        {
            throw std::runtime_error("Failed to set control server options");
        }

        if (bind(sockfd, (const sockaddr *)&addr, sizeof(addr)) != 0)
        {
            throw std::runtime_error("Control server failed to bind to port " +
                                     std::to_string(port));
        }

        if (listen(sockfd, 4) != 0)
        {
            throw std::runtime_error(
                "Control server failed to enter listenning state.");
        }
    }

    ControlCommands GetNextCommand(char *buf, std::size_t size) const
    {
        // one command per connection
        // we don't care about the address, as it must be local (is it
        // secure?)

        fd_set rfds;
        int val;
        struct timeval timeout;
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);

        timeout.tv_sec = 1;

        val = select(sockfd + 1, &rfds, nullptr, nullptr, &timeout);

        if (val <= 0){
            return ControlCommands::NONE;
        }

        int connfd = accept(sockfd, nullptr, nullptr);

        if (recv(connfd, buf, size, 0) <= 0)
        {
            // disconnected
            return ControlCommands::NONE;
        }

        auto cmd = static_cast<ControlCommands>(buf[0] - '0');
        close(connfd);

        return cmd;
    }

   private:
    int sockfd;
};

#endif

// TODO: set SO_PRIORITY