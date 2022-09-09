#ifndef STRATUM_SERVER_ZEC_HPP_
#define STRATUM_SERVER_ZEC_HPP_

#include "static_config.hpp"
#include "stratum_server.hpp"
#include "share.hpp"

class StratumServerBtc : public StratumServer
{
    public:
     using share_t = ShareBtc;
     StratumServerBtc(const CoinConfig& conf) : StratumServer(conf) {}

    private: 

    RpcResult HandleAuthorize(StratumClient* cli,
                              simdjson::ondemand::array& params);
    RpcResult HandleSubscribe(StratumClient* cli,
                              simdjson::ondemand::array& params) const;
    RpcResult HandleSubmit(StratumClient* cli, WorkerContext* wc,
                           simdjson::ondemand::array& params);

    void HandleReq(Connection<StratumClient>* conn, WorkerContext* wc,
                   std::string_view req) override;
    void UpdateDifficulty(Connection<StratumClient>* conn) override;
};

using stratum_server_t = StratumServerBtc;

#endif