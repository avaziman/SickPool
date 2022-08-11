#include "stratum_client.hpp"

uint32_t StratumClient::extra_nonce_counter = 0;

StratumClient::StratumClient(const int sock, const std::string& ip, const int64_t time, const double diff)
    : sock(sock),
      ip(ip),
      connect_time(time),
      last_adjusted(time),
      last_share_time(time),
      current_diff(diff),
      pending_diff(diff)
{
    extra_nonce = extra_nonce_counter++;
    extra_nonce_str = fmt::format("{:08x}", extra_nonce);
}