#ifndef STRATUM_SERVER_ZEC_HPP_
#define STRATUM_SERVER_ZEC_HPP_

#include "stratum_server.hpp"
#include "stratum_client.hpp"
#include "share.hpp"

class StratumServerZec : public StratumServer
{
   public:
    using share_t = ShareZec;

    StratumServerZec(const CoinConfig& conf) : StratumServer(conf) {}

    RpcResult HandleAuthorize(StratumClient* cli,
                              simdjson::ondemand::array& params);
    RpcResult HandleSubscribe(StratumClient* cli,
                              simdjson::ondemand::array& params) const;
    RpcResult HandleSubmit(StratumClient* cli, WorkerContext* wc,
                           simdjson::ondemand::array& params);

    void HandleReq(StratumClient* cli, WorkerContext* wc,
                   std::string_view req) override;
    void UpdateDifficulty(StratumClient* cli) override;
};

using stratum_server_t = StratumServerZec;

#endif