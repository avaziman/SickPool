#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#include <fcntl.h>
#include <simdjson.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <any>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../sock_addr.hpp"
#include "benchmark.hpp"
#include "blocks/block_submission.hpp"
#include "blocks/block_submission_manager.hpp"
#include "coin_config.hpp"
#include "control/control_server.hpp"
#include "jobs/job_manager.hpp"
#include "jobs/verus_job.hpp"
#include "redis/redis_manager.hpp"
#include "shares/share_processor.hpp"
#include "static_config/config.hpp"
#include "stats_manager.hpp"
#include "stratum_client.hpp"

#define MAX_HTTP_REQ_SIZE (MAX_BLOCK_SIZE * 2)
#define MAX_HTTP_JSON_DEPTH 3

#define SOCK_TIMEOUT 5
#define MIN_PERIOD_SECONDS 20
#define MAX_CONNECTIONS 1e5
#define MAX_CONNECTION_EVENTS 10
#define EPOLL_TIMEOUT -1

class StratumServer
{
   public:
    StratumServer(const CoinConfig& conf);
    ~StratumServer();
    void StartListening();

   private:
    CoinConfig coin_config;
    std::string chain{"VRSCTEST"};

    int listening_fd;

    simdjson::ondemand::parser httpParser =
        simdjson::ondemand::parser(MAX_HTTP_REQ_SIZE);

    ControlServer control_server;
    RedisManager redis_manager;
    StatsManager stats_manager;
    DaemonManager daemon_manager;
    JobManager job_manager;
    SubmissionManager submission_manager;
    RoundManager round_manager_pow;
    RoundManager round_manager_pos;
    DifficultyManager diff_manager;

    std::unordered_map<int, std::unique_ptr<StratumClient>> clients;

    std::mutex jobs_mutex;
    std::mutex clients_mutex;
    std::mutex redis_mutex;

    void HandleControlCommands();
    void HandleControlCommand(ControlCommands cmd, char* buff);

    void Listen();
    void HandleSocket(int sockfd);
    void HandleReq(StratumClient* cli, WorkerContext* wc,
                   std::string_view req);
    void HandleBlockNotify();
    void HandleWalletNotify(WalletNotify* wal_notify);

    void HandleSubscribe(const StratumClient* cli, int id,
                         simdjson::ondemand::array& params) const;
    void HandleAuthorize(StratumClient* cli, int id,
                         simdjson::ondemand::array& params);
    void HandleSubmit(StratumClient* cli, WorkerContext* wc, int id,
                      simdjson::ondemand::array& params);

    void HandleReadySocket(int sockfd, WorkerContext *wc);
    void HandleShare(StratumClient* cli, WorkerContext* wc, int id, Share& share);
    void SendReject(const StratumClient* cli, int id, int error,
                    const char* msg) const;
    void SendAccept(const StratumClient* cli, int id) const;

    void UpdateDifficulty(StratumClient* cli);

    void BroadcastJob(const StratumClient* cli, const Job* job) const;
    int AcceptConnection(int epfd, int listen_fd, sockaddr_in* addr,
                         socklen_t* addr_size);
    int CreateListeningSock(int epfd);
    void ServiceSockets(int epfd, int listener_fd);
    void AddClient(int sockfd);
    StratumClient* GetClient(int sockfd);

    inline std::size_t SendRaw(int sock, const char* data,
                               std::size_t len) const
    {
        // dont send sigpipe
        return send(sock, data, len, MSG_NOSIGNAL);
    }
};
#endif