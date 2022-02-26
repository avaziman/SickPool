#ifndef STRATUM_CLIENT_HPP_
#define STRATUM_CLIENT_HPP_

class StratumClient
{
   public:
    StratumClient(int sock, uint32_t time, double diff)
        : sockfd(sock), connect_time(time), difficulty(diff)
    {
        extra_nonce = 10;
        ToHex(extra_nonce_str, extra_nonce);
        extra_nonce_str[8] = 0;
    }

    int GetSock() { return sockfd; }
    double GetDifficulty() { return difficulty; }
    const char* GetExtraNonce() { return extra_nonce_str; }

   private:
    int sockfd;
    uint32_t connect_time;
    uint32_t extra_nonce;
    char extra_nonce_str[9];
    double difficulty;
};

#endif