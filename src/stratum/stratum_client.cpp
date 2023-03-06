#include "stratum_client.hpp"

uint32_t StratumClient::extra_nonce_counter = 0;

StratumClient::StratumClient(const int64_t time, const double diff,
                             const double rate, uint32_t retarget_interval)
    : VarDiff(rate, retarget_interval),
      connect_time(time),
      extra_nonce(extra_nonce_counter++),
      extra_nonce_hex(Hexlify(extra_nonce)),
      extra_nonce_sv(extra_nonce_hex.data(), sizeof(extra_nonce_hex)),
      current_diff(diff)
{
}