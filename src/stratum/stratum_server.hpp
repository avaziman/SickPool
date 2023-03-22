#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#include <simdjson.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "base58.h"
#include "block_submitter.hpp"
#include "cn/common/base58.h"
#include "connection.hpp"
#include "job.hpp"
#include "job_vrsc.hpp"
#include "jobs/job_manager.hpp"
#include "logger.hpp"
#include "server.hpp"
#include "shares/share_processor.hpp"
#include "static_config/static_config.hpp"
#include "stratum_client.hpp"
#include "stratum_server_base.hpp"

struct Field
{
    std::string_view name;
    std::string_view* value;
    std::size_t size;
};

inline simdjson::error_code GetArrField(simdjson::ondemand::array_iterator& IT,
                                        std::string_view& FIELD,
                                        std::size_t FIELD_SIZE)
{
    auto res = (*IT).get<std::string_view>().get(FIELD);

    if (res == simdjson::error_code::SUCCESS)
    {
        if (FIELD.size() != FIELD_SIZE && FIELD_SIZE != 0)
        {
            FIELD = "Bad size of field";
            return simdjson::error_code::EMPTY;
        }
    }

    return res;
}

template <size_t size>
inline std::string ParseShareParams(std::array<Field, size> fields,
                                    simdjson::ondemand::array& params)
{
    const simdjson::ondemand::array_iterator json_end = params.end();

    auto fields_it = fields.begin();
    for (simdjson::ondemand::array_iterator json_it = params.begin();
         json_it != json_end && fields_it != fields.end();
         ++json_it, ++fields_it)
    {
        if (GetArrField(json_it, *(*fields_it).value, (*fields_it).size) !=
            simdjson::error_code::SUCCESS)
        {
            // logger.template Log<LogType::Critical>("Failed to parse submit:
            // {}",
            //                                        (*fields_it).name);
            // return RpcResult(ResCode::UNKNOWN,
            // std::string(*(*fields_it).value));
            return std::string((*fields_it).name) + ": " +
                   std::string(*(*fields_it).value);
        }
    }
    return std::string{};
}

template <StaticConf confs>
class StratumServer : public StratumBase, public StratumConstants
{
   public:
    explicit StratumServer(CoinConfig&& conf);
    ~StratumServer() override;
    using WorkerContextT = WorkerContext<confs.BLOCK_HEADER_SIZE>;
    using JobT = Job<confs.STRATUM_PROTOCOL>;
    using ShareT = StratumShareT<confs.STRATUM_PROTOCOL>;

   private:
    static constexpr std::string_view field_str_stratum = "StratumServer";
    const Logger logger{field_str_stratum};

    std::jthread stats_thread;
    simdjson::ondemand::parser httpParser =
        simdjson::ondemand::parser(HTTP_REQ_ALLOCATE);

   protected:
    JobManager<JobT, confs.COIN_SYMBOL> job_manager;
    DaemonManagerT<confs.COIN_SYMBOL> daemon_manager;
    BlockSubmitter<confs.COIN_SYMBOL> block_submitter;
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

    RpcResult HandleShare(Connection<StratumClient>* con, WorkerContextT* wc,
                          ShareT& share);

    virtual void UpdateDifficulty(Connection<StratumClient>* conn) = 0;

    virtual void BroadcastJob(Connection<StratumClient>* conn, double diff,
                              const JobT* job) const = 0;
    void HandleConsumeable(connection_it* conn) override;
    bool HandleConnected(connection_it* conn) override;
    bool HandleTimeout(connection_it* conn, uint64_t timeout_streak) override;

    void DisconnectClient(
        const std::shared_ptr<Connection<StratumClient>> conn_ptr) override;
};

#endif