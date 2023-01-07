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

#include "cn/common/base58.h"
#include "block_submitter.hpp"
#include "connection.hpp"
#include "job.hpp"
#include "jobs/job_manager.hpp"
#include "logger.hpp"
#include "server.hpp"
#include "shares/share_processor.hpp"
#include "static_config/static_config.hpp"
#include "stratum_client.hpp"
#include "stratum_server_base.hpp"

template <StaticConf confs>
class StratumServer : public StratumBase
{
   public:
    explicit StratumServer(CoinConfig&& conf);
    ~StratumServer() override;
    using WorkerContextT = WorkerContext<confs.BLOCK_HEADER_SIZE>;
    using JobT = Job<confs.STRATUM_PROTOCOL>;

   private:
    using ShareT = StratumShare<confs.STRATUM_PROTOCOL>;

    static constexpr std::string_view field_str_stratum = "StratumServer";
    const Logger<field_str_stratum> logger;

    std::jthread stats_thread;
    simdjson::ondemand::parser httpParser =
        simdjson::ondemand::parser(HTTP_REQ_ALLOCATE);

   protected:

    daemon_manager_t daemon_manager;
    PayoutManager payout_manager;
    job_manager_t job_manager;
    BlockSubmitter block_submitter;
    StatsManager stats_manager;

    virtual void HandleReq(Connection<StratumClient>* conn, WorkerContextT* wc,
                           std::string_view req) = 0;

    virtual RpcResult HandleAuthorize(StratumClient* cli,
                                      std::string_view miner,
                                      std::string_view worker);
    RpcResult HandleAuthorize(StratumClient* cli,
                              simdjson::ondemand::array& params);

    void HandleBlockNotify() override;
    void HandleNewJob() override;
    void HandleNewJob(const std::shared_ptr<JobT> new_job);

    RpcResult HandleShare(Connection<StratumClient>* con,
                          WorkerContextT* wc, ShareT& share);

    virtual void UpdateDifficulty(Connection<StratumClient>* conn) = 0;

    virtual void BroadcastJob(Connection<StratumClient>* conn,
                              const JobT* job) const = 0;
    void HandleConsumeable(connection_it* conn) override;
    bool HandleConnected(connection_it* conn) override;

    void DisconnectClient(
        const std::shared_ptr<Connection<StratumClient>> conn_ptr) override;
};

#endif