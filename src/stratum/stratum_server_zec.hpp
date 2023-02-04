#ifndef STRATUM_SERVER_ZEC_HPP_
#define STRATUM_SERVER_ZEC_HPP_

#include "stratum_server.hpp"
#include "stratum_client.hpp"
#include "share.hpp"
#include "config_vrsc.hpp"

static constexpr std::string_view field_str_zec = "StratumServerZec";

template <StaticConf confs>
class StratumServerZec : public StratumServer<confs>, public CoinConstantsZec
{
   public:
    explicit StratumServerZec(CoinConfig&& conf)
        : StratumServer<confs>(std::move(conf))
    {
    }
   private:
    using ShareT = StratumServer<confs>::ShareT;
    using WorkerContextT = StratumServer<confs>::WorkerContextT;
    using JobT = StratumServer<confs>::JobT;

    const Logger<field_str_zec> logger;

    RpcResult HandleSubscribe(StratumClient* cli,
                              simdjson::ondemand::array& params) const;
    RpcResult HandleSubmit(Connection<StratumClient>* con, WorkerContextT* wc,
                           simdjson::ondemand::array& params);

    void HandleReq(Connection<StratumClient>* conn, WorkerContextT* wc,
                   std::string_view req) override;
    void UpdateDifficulty(Connection<StratumClient>* conn) override;

    void BroadcastJob(Connection<StratumClient>* conn, double diff,
                              const JobT* job) const override;
};

#endif