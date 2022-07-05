#include "stratum_client.hpp"

uint32_t StratumClient::extra_nonce_counter = 0;

StratumClient::StratumClient(const int sock, const int64_t time, const double diff)
    : sockfd(sock),
      connect_time(time),
      last_adjusted(time),
      last_share_time(time),
      current_diff(diff),
      pending_diff(diff)
{
    extra_nonce = extra_nonce_counter++;
    char buff[8];
    ToHex(buff, extra_nonce);
    extra_nonce_str = std::string(buff, sizeof(buff));
}