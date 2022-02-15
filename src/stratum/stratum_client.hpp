#ifndef STRATUM_CLIENT_HPP
#define STRATUM_CLIENT_HPP

#include "../crypto/verushash/uint256.h"
#include <unistd.h>

#include <sstream>
#include <string>
#include <iomanip>

class StratumClient
{
   public:
    StratumClient(int sock, uint32_t created, double diff)
        : sockfd(sock), createdTime(created), difficulty(diff)
    {
        extra_nonce = 100;
    }
    ~StratumClient()
    {
        close(sockfd);
    }
    std::string GetExtraNonce()
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8) << extra_nonce;
        return ss.str();
    }

    double GetDifficulty() { return difficulty; }

    int GetSock() { return sockfd; }

   private:
    int sockfd;
    double difficulty;
    uint32_t createdTime;
    uint32_t extra_nonce;  // aka extranonce
};

#endif