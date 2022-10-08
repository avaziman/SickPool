#ifndef STRATUM_SERVER_ZEC_HPP_
#define STRATUM_SERVER_ZEC_HPP_

#include "static_config.hpp"
#include "stratum_server.hpp"
#include "job_cryptonote.hpp"
#include "share.hpp"
#include "cn/currency_protocol/blobdatatype.h"
#include "cn/common/base58.h"

class StratumServerCn : public StratumServer
{
    public:
     using share_t = ShareCn;
     StratumServerCn(const CoinConfig& conf) : StratumServer(conf) {}

    private: 

    RpcResult HandleAuthorize(StratumClient* cli,
                              simdjson::ondemand::array& params, std::string_view worker);
    RpcResult HandleSubscribe(StratumClient* cli,
                              simdjson::ondemand::array& params) const;
    RpcResult HandleSubmit(StratumClient* cli, WorkerContext* wc,
                           simdjson::ondemand::array& params,
                           std::string_view worker);

    void HandleReq(Connection<StratumClient>* conn, WorkerContext* wc,
                   std::string_view req) override;
    void UpdateDifficulty(Connection<StratumClient>* conn) override;

    void BroadcastJob(Connection<StratumClient>* conn,
                                       const job_t* job, int id) const;
 
    void BroadcastJob(Connection<StratumClient>* conn, const job_t* job) const override;
};

using stratum_server_t = StratumServerCn;

#endif