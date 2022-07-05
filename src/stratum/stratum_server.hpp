#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#include <simdjson.h>
#include <sys/socket.h>

#include <any>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../crypto/hash_wrapper.hpp"
#include "../logger.hpp"
#include "../sock_addr.hpp"
#include "../stats_manager.hpp"
#include "block_submission.hpp"
#include "coin_config.hpp"
#include "control_server.hpp"
#include "job_manager.hpp"
#include "redis_manager.hpp"
#include "share.hpp"
#include "share_processor.hpp"
#include "static_config/config.hpp"
#include "stratum_client.hpp"
#include "submission_manager.hpp"
#include "verus_job.hpp"

#define MAX_HTTP_REQ_SIZE (MAX_BLOCK_SIZE * 2)
#define MAX_HTTP_JSON_DEPTH 3

#define SOCK_TIMEOUT 5
#define MIN_PERIOD_SECONDS 20

class StratumServer
{
   public:
    StratumServer(const CoinConfig& conf);
    ~StratumServer();
    void StartListening();
    

   private:
    CoinConfig coin_config;
    std::string chain{"VRSCTEST"};

    int sockfd;
    struct sockaddr_in addr;

    simdjson::ondemand::parser httpParser =
        simdjson::ondemand::parser(MAX_HTTP_REQ_SIZE);

    ControlServer control_server;
    RedisManager redis_manager;
    StatsManager stats_manager;
    DaemonManager daemon_manager;
    JobManager job_manager;
    SubmissionManager submission_manager;
    // DifficultyManager* diff_manager;

    std::vector<std::unique_ptr<StratumClient>> clients;

    std::mutex jobs_mutex;
    std::mutex clients_mutex;
    std::mutex redis_mutex;

    void HandleControlCommands();
    void HandleControlCommand(ControlCommands cmd);

    void Listen();
    void HandleSocket(int sockfd);
    void HandleReq(StratumClient* cli, char buffer[], std::size_t reqSize);
    void HandleBlockNotify(const simdjson::ondemand::array& params);
    void HandleWalletNotify(simdjson::ondemand::array& params);

    void HandleSubscribe(const StratumClient* cli, int id,
                         simdjson::ondemand::array& params) const;
    void HandleAuthorize(StratumClient* cli, int id,
                         simdjson::ondemand::array& params);
    void HandleSubmit(StratumClient* cli, int id,
                      simdjson::ondemand::array& params);

    void HandleShare(StratumClient* cli, int id, Share& share);
    void SendReject(const StratumClient* cli, int id, int error,
                    const char* msg);
    void SendAccept(const StratumClient* cli, int id);
    bool SubmitBlock(std::string_view block_hex);

    void UpdateDifficulty(StratumClient* cli);
    void AdjustDifficulty(StratumClient* cli, int64_t curTime);

    void BroadcastJob(const StratumClient* cli, const Job* job) const;

    inline std::size_t SendRaw(int sock, const char* data,
                                       std::size_t len) const
    {
        // dont send sigpipe
        return send(sock, data, len, MSG_NOSIGNAL);
    }
};
#endif