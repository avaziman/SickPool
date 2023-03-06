#ifndef STRATUM_SERVER_BTC_HPP_
#define STRATUM_SERVER_BTC_HPP_

#include "static_config.hpp"
#include "stratum_server.hpp"
#include "share.hpp"
static constexpr std::string_view field_str_btc = "StratumServerBtc";

template <StaticConf confs>
class StratumServerBtc : public StratumServer<confs>
{
    public:
     using WorkerContextT = StratumServer<confs>::WorkerContextT;
     using JobT = StratumServer<confs>::JobT;

     explicit StratumServerBtc(CoinConfig&& conf)
         : StratumServer<confs>(std::move(conf))
     {
     }

    private:
     const Logger logger{field_str_btc};

     RpcResult HandleSubscribe(StratumClient* cli,
                               simdjson::ondemand::array& params) const;
     RpcResult HandleSubmit(StratumClient* cli, WorkerContextT* wc,
                            simdjson::ondemand::array& params);

     void HandleReq(Connection<StratumClient>* conn, WorkerContextT* wc,
                    std::string_view req) override;
     void UpdateDifficulty(Connection<StratumClient>* conn) override;

     void BroadcastJob(Connection<StratumClient>* conn, const JobT* job) const;
};

#endif