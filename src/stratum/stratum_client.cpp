#include "stratum_client.hpp"

uint32_t StratumClient::extra_nonce_counter = 0;

StratumClient::StratumClient(const int64_t time, const double diff)
    : connect_time(time),
      last_adjusted(time),
      last_share_time(time),
      current_diff(diff),
      pending_diff(diff),
      extra_nonce(extra_nonce_counter++),
      extra_nonce_sv(extra_nonce_hex, sizeof(extra_nonce_hex)),
      id(0,0)
{
    fmt::format_to_n(extra_nonce_hex, sizeof(extra_nonce_hex), "{:08x}",
                     extra_nonce);
}