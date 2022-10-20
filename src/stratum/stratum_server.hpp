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

#include "block_submitter.hpp"
#include "../sock_addr.hpp"
#include "benchmark.hpp"
#include "blocks/block_submission.hpp"
#include "connection.hpp"
#include "control/control_server.hpp"
#include "jobs/job_manager.hpp"
#include "logger.hpp"
#include "redis/redis_manager.hpp"
#include "server.hpp"
#include "shares/share_processor.hpp"
#include "static_config/static_config.hpp"
#include "stats_manager.hpp"
#include "stratum_client.hpp"

class StratumServer : public Server<StratumClient>
{
   public:
    StratumServer(const CoinConfig& conf);
    ~StratumServer();
    void Listen();
    void Stop();

   protected:
    CoinConfig coin_config;
    std::string chain{COIN_SYMBOL};
    Logger<LogField::Stratum> logger;

    simdjson::ondemand::parser httpParser =
        simdjson::ondemand::parser(HTTP_REQ_ALLOCATE);

    std::jthread control_thread;
    std::jthread stats_thread;
    std::vector<std::jthread> processing_threads;

    job_manager_t job_manager;
    ControlServer control_server;
    RedisManager redis_manager;
    StatsManager stats_manager;
    daemon_manager_t daemon_manager;
    DifficultyManager diff_manager;
    PaymentManager payment_manager;
    BlockSubmitter block_submitter;

    RoundManager round_manager;

    // O(log n) delete + insert
    // saving the pointer in epoll gives us O(1) access!
    // allows us to sort connections by hashrate to minimize loss
    std::map<std::shared_ptr<Connection<StratumClient>>, double> clients;
    std::shared_mutex clients_mutex;

    void HandleControlCommands(std::stop_token st);
    void HandleControlCommand(ControlCommands cmd, char* buff);

    virtual void HandleReq(Connection<StratumClient>* conn, WorkerContext* wc,
                           std::string_view req) = 0;
    void HandleBlockNotify();
    // void HandleWalletNotify(WalletNotify* wal_notify);

    RpcResult HandleShare(StratumClient* cli, WorkerContext* wc,
                          share_t& share);

    virtual void UpdateDifficulty(Connection<StratumClient>* conn) = 0;

    virtual void BroadcastJob(Connection<StratumClient>* conn,
                              const job_t* job) const = 0;
    int AcceptConnection(sockaddr_in* addr, socklen_t* addr_size);
    void InitListeningSock();
    // void EraseClient(int sockfd,
    //                  std::list<std::unique_ptr<StratumClient>>::iterator*
    //                  it);
    void ServiceSockets(std::stop_token st);
    void HandleConsumeable(connection_it* conn) override;
    void HandleConnected(connection_it* conn) override;
    void HandleDisconnected(connection_it* conn) override;

    void DisconnectClient(
        const std::shared_ptr<Connection<StratumClient>> conn_ptr);
    inline void SendRes(int sock, int req_id, const RpcResult& res)
    {
        char buff[512];
        size_t len = 0;

        if (res.code == ResCode::OK)
        {
            len =
                fmt::format_to_n(buff, sizeof(buff),
                                 "{{\"id\":{},\"result\":{},\"error\":null}}\n",
                                 req_id, res.msg)
                    .size;
        }
        else
        {
            len = fmt::format_to_n(buff, sizeof(buff),
                                   "{{\"id\":{},\"result\":null,\"error\":[{},"
                                   "\"{}\",null]}}\n",
                                   req_id, (int)res.code, res.msg)
                      .size;
        }

        SendRaw(sock, buff, len);
    }

    inline std::size_t SendRaw(int sock, const char* data,
                               std::size_t len) const
    {
        // dont send sigpipe
        auto res = send(sock, data, len, MSG_NOSIGNAL);

        if (res == -1)
        {
            logger.Log<LogType::Error>(
                        "Failed to send on sock fd {}, errno: {} -> {}", sock,
                        errno, std::strerror(errno));
        }

        return res;
    }
};

#ifdef STRATUM_PROTOCOL_ZEC
#include "stratum_server_zec.hpp"
#elif STRATUM_PROTOCOL_BTC
#include "stratum_server_btc.hpp"
#elif STRATUM_PROTOCOL_CN
#include "stratum_server_cn.hpp"
#endif

#endif