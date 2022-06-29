#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#include <simdjson.h>
#include <sys/socket.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <deque>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../coin_config.hpp"
#include "../crypto/hash_wrapper.hpp"
#include "../crypto/merkle_tree.hpp"
#include "../crypto/transaction.hpp"
#include "../crypto/verus_header.hpp"
#include "../crypto/verus_transaction.hpp"
#include "../daemon/daemon_rpc.hpp"
#include "../logger.hpp"
#include "../sock_addr.hpp"
#include "../stats_manager.hpp"
#include "byteswap.h"
#include "job_manager.hpp"
#include "control_server.hpp"
#include "redis_manager.hpp"
#include "round.hpp"
#include "share.hpp"
#include "share_processor.hpp"
#include "stratum_client.hpp"
#include "verus_job.hpp"
#include "block_submission.hpp"

#define MAX_HTTP_REQ_SIZE (MAX_BLOCK_SIZE * 2)
#define MAX_HTTP_JSON_DEPTH 3

#define REQ_BUFF_SIZE (1024 * 32)
#define REQ_BUFF_SIZE_REAL (REQ_BUFF_SIZE - SIMDJSON_PADDING)
#define SOCK_TIMEOUT 5
#define MIN_PERIOD_SECONDS 20

using namespace simdjson;
using namespace std::chrono;

class StratumServer
{
   public:
    StratumServer();
    ~StratumServer();
    void StartListening();
    static CoinConfig coin_config;
    static std::vector<DaemonRpc*> rpcs;
    static int SendRpcReq(std::string& result, int id, const char* method,
                          const char* params, std::size_t paramsLen);

   private:
    int sockfd;
    struct sockaddr_in addr;
    std::string_view last_job_id_hex;

    ondemand::parser reqParser;
    ondemand::parser httpParser;

    ControlServer control_server;
    RedisManager redis_manager;
    StatsManager stats_manager;
    static JobManager job_manager;
    // DifficultyManager* diff_manager;

    std::vector<std::unique_ptr<StratumClient>> clients;

    std::deque<BlockSubmission> block_submissions;
    // hash + block id
    std::vector<ImmatureSubmission> immature_block_submissions;

    // job id hex str -> job, O(1) job lookup
    std::unordered_map<std::string_view, job_t*> jobs;
    // chain name str -> chain round

    uint32_t block_number = 0;
    int64_t mature_timestamp_ms = 0;
    int64_t last_block_timestamp_map = 0;  // TODO: make map

    std::mutex jobs_mutex;
    std::mutex clients_mutex;
    std::mutex redis_mutex;

    static std::mutex rpc_mutex;

    void HandleControlCommands();
    void HandleControlCommand(ControlCommands cmd);

    void Listen();
    void HandleSocket(int sockfd);
    void HandleReq(StratumClient* cli, char buffer[], int reqSize);
    void HandleBlockNotify(const ondemand::array& params);
    void HandleWalletNotify(ondemand::array& params);

    void HandleSubscribe(StratumClient* cli, int id, ondemand::array& params);
    void HandleAuthorize(StratumClient* cli, int id, ondemand::array& params);
    void HandleSubmit(StratumClient* cli, int id, ondemand::array& params);

    void HandleShare(StratumClient* cli, int id, Share& share);
    void SendReject(StratumClient* cli, int id, int error, const char* msg);
    void SendAccept(StratumClient* cli, int id);
    bool SubmitBlock(const char* blockHex, int blockHexLen);

    void UpdateDifficulty(StratumClient* cli);
    void AdjustDifficulty(StratumClient* cli, int64_t curTime);

    void BroadcastJob(const StratumClient* cli, Job* job);

    inline std::size_t SendRaw(int sock, const char* data, std::size_t len) const;
};
#endif