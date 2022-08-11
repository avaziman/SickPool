#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#include <fcntl.h>
#include <netinet/tcp.h>
#include <simdjson.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <iterator>
#include <list>
#include <any>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../sock_addr.hpp"
#include "benchmark.hpp"
#include "blocks/block_submission.hpp"
#include "blocks/block_submission_manager.hpp"
#include "client_list.hpp"
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
#define MAX_CONNECTIONS 32
#define MAX_CONNECTION_EVENTS 10
#define EPOLL_TIMEOUT 1  // ms
#define RETRIES 10

class StratumServer
{
   public:
    StratumServer(const CoinConfig& conf);
    ~StratumServer();
    void Listen();
    void Stop();

   private:
    CoinConfig coin_config;
    std::string chain{"VRSCTEST"};

    int listening_fd;
    int epoll_fd;

    simdjson::ondemand::parser httpParser =
        simdjson::ondemand::parser(MAX_HTTP_REQ_SIZE);

    std::jthread control_thread;
    std::jthread stats_thread;
    std::vector<std::jthread> processing_threads;

    ControlServer control_server;
    RedisManager redis_manager;
    StatsManager stats_manager;
    DaemonManager daemon_manager;
    JobManager job_manager;
    SubmissionManager submission_manager;
    DifficultyManager diff_manager;
    PaymentManager payment_manager;

    RoundManager round_manager_pow;
    RoundManager round_manager_pos;

    // O(1) delete + insert + no invalidation, 
    // saving the pointer in epoll gives us O(1) access!
    std::list<std::unique_ptr<StratumClient>> clients;

    std::mutex jobs_mutex;
    std::mutex clients_mutex;
    std::mutex redis_mutex;

    void HandleControlCommands(std::stop_token st);
    void HandleControlCommand(ControlCommands cmd, char* buff);

    void HandleReq(StratumClient* cli, WorkerContext* wc, std::string_view req);
    void HandleBlockNotify();
    void HandleWalletNotify(WalletNotify* wal_notify);

    void HandleSubscribe(StratumClient* cli, int id,
                         simdjson::ondemand::array& params) const;
    void HandleAuthorize(StratumClient* cli, int id,
                         simdjson::ondemand::array& params);
    void HandleSubmit(StratumClient* cli, WorkerContext* wc, int id,
                      simdjson::ondemand::array& params);

    void HandleNewConnection();
    void HandleReadableSocket(
        std::list<std::unique_ptr<StratumClient>>::iterator* it, WorkerContext* wc);
    void HandleShare(StratumClient* cli, WorkerContext* wc, int id,
                     Share& share);
    void SendReject(StratumClient* cli, int id, int error,
                    const char* msg) const;
    void SendAccept(StratumClient* cli, int id) const;

    void UpdateDifficulty(StratumClient* cli);

    void BroadcastJob(StratumClient* cli, const Job* job) const;
    int AcceptConnection(sockaddr_in* addr, socklen_t* addr_size);
    int CreateListeningSock();
    void ServiceSockets(std::stop_token st);
    std::list<std::unique_ptr<StratumClient>>::iterator* AddClient(
        int sockfd, const std::string& ip);
    void EraseClient(int sockfd,
                     std::list<std::unique_ptr<StratumClient>>::iterator* it);

    inline std::size_t SendRaw(int& sock, const char* data,
                               std::size_t len) const
    {
        // dont send sigpipe
        auto res = send(sock, data, len, MSG_NOSIGNAL);

        if(res == -1){
            Logger::Log(LogType::Error, LogField::Stratum,
                        "Failed to send on sock fd {}, errno: {} -> {}", sock,
                        errno, std::strerror(errno));
        }

        return res;
    }
};
#endif