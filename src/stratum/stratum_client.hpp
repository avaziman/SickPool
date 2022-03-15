#ifndef STRATUM_CLIENT_HPP_
#define STRATUM_CLIENT_HPP_
#include <cstring>

#include "../crypto/utils.hpp"

class StratumClient
{
   public:
    StratumClient(int sock, std::time_t time, double diff)
        : sockfd(sock),
          connect_time(time),
          last_adjusted(time),
          last_share_time(time),
          current_diff(diff),
          last_diff(diff)
    {
        extra_nonce = 10;
        ToHex(extra_nonce_str, extra_nonce);
        extra_nonce_str[8] = 0;
    }

    int GetSock() { return sockfd; }
    double GetDifficulty() { return current_diff; }
    void SetDifficulty(double diff, std::time_t curTime)
    {
        last_diff = current_diff;
        current_diff = diff;
        last_adjusted = curTime;
    }

    const char* GetExtraNonce() { return extra_nonce_str; }
    uint32_t GetShareCount() { return share_count; }
    void ResetShareCount() { share_count = 0; }
    std::time_t GetLastAdjusted() { return last_adjusted; }

    void SetLastShare(std::time_t time)
    {
        last_share_time = time;
        share_count++;
    }

   private:
    int sockfd;
    uint32_t extra_nonce;
    std::time_t connect_time;
    std::time_t last_adjusted;
    std::time_t last_share_time;
    uint32_t share_count = 0;
    char extra_nonce_str[9];
    double current_diff;
    double last_diff;
};

#endif