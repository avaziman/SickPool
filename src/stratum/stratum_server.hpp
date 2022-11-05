#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#include <simdjson.h>

#include <chrono>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "../sock_addr.hpp"
#include "benchmark.hpp"
#include "block_submitter.hpp"
#include "blocks/block_submission.hpp"
#include "connection.hpp"
#include "jobs/job_manager.hpp"
#include "logger.hpp"
#include "server.hpp"
#include "shares/share_processor.hpp"
#include "static_config/static_config.hpp"
#include "stratum_client.hpp"
#include "stratum_server_base.hpp"
static constexpr std::string_view field_str_stratum = "StratumServer";

template <StaticConf confs>
class StratumServer : public StratumBase
{
   public:
    explicit StratumServer(CoinConfig&& conf);
    ~StratumServer() override;

   private:
    const Logger<field_str_stratum> logger;

   protected:
    simdjson::ondemand::parser httpParser =
        simdjson::ondemand::parser(HTTP_REQ_ALLOCATE);

    job_manager_t job_manager;
    daemon_manager_t daemon_manager;
    PaymentManager payment_manager;
    BlockSubmitter block_submitter;

    virtual void HandleReq(Connection<StratumClient>* conn, WorkerContext* wc,
                           std::string_view req) = 0;
    void HandleBlockNotify() override;
    // void HandleWalletNotify(WalletNotify* wal_notify);

    RpcResult HandleShare(StratumClient* cli, WorkerContext* wc,
                          share_t& share);

    virtual void UpdateDifficulty(Connection<StratumClient>* conn) = 0;

    virtual void BroadcastJob(Connection<StratumClient>* conn,
                              const job_t* job) const = 0;
    // void EraseClient(int sockfd,
    //                  std::list<std::unique_ptr<StratumClient>>::iterator*
    //                  it);
    void HandleConsumeable(connection_it* conn) override;
    bool HandleConnected(connection_it* conn) override;
};

#ifdef STRATUM_PROTOCOL_ZEC
#include "stratum_server_zec.hpp"
#elif STRATUM_PROTOCOL_BTC
#include "stratum_server_btc.hpp"
#elif STRATUM_PROTOCOL_CN
#include "stratum_server_cn.hpp"
#endif

#endif