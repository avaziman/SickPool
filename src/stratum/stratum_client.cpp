#include "stratum_client.hpp"

uint32_t StratumClient::extra_nonce_counter = 0;

//TODO: diff
StratumClient::StratumClient(const int sock, const std::string& ip,
                             const int64_t time)
    : sock(sock),
      ip(ip),
      connect_time(time),
      last_adjusted(time),
      last_share_time(time),
      current_diff(1000000),
      pending_diff(100000)
{
    extra_nonce = extra_nonce_counter++;
    extra_nonce_str = fmt::format("{:08x}", extra_nonce);
}