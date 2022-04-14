#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#include <simdjson.h>
#include <sw/redis++/redis++.h>
#include <sys/socket.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <deque>

#include "./job_manager.hpp"
#include "../coin_config.hpp"
#include "../crypto/block.hpp"
#include "../crypto/block_header.hpp"
#include "../crypto/hash_wrapper.hpp"
#include "../crypto/merkle_tree.hpp"
#include "../crypto/transaction.hpp"
#include "../crypto/verus_header.hpp"
#include "../crypto/verus_transaction.hpp"
#include "../crypto/verushash/verus_hash.h"
#include "../daemon/daemon_rpc.hpp"
#include "../logger.hpp"
#include "../sock_addr.hpp"
#include "./difficulty_manager.hpp"
#include "./job_manager.hpp"
#include "byteswap.h"
#include "deque"
#include "job.hpp"
#include "redis_manager.hpp"
#include "share.hpp"
#include "share_processor.hpp"
#include "stratum_client.hpp"
#include "verus_job.hpp"

#define REQ_BUFF_SIZE_REAL (REQ_BUFF_SIZE - SIMDJSON_PADDING)
#define MAX_HTTP_REQ_SIZE (MAX_BLOCK_SIZE * 2)
#define MAX_HTTP_JSON_DEPTH 3

#define REQ_BUFF_SIZE (1024 * 32)
#define SOCK_TIMEOUT 5
#define SOLUTION_SIZE 1344
#define MIN_PERIOD_SECONDS 20

using namespace sw::redis;

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
    static int SendRpcReq(std::vector<char>& result, int id, const char* method,
                          const char* params, int paramsLen);

   private:

    int sockfd;
    struct sockaddr_in addr;
    uint32_t job_count;
    double target_shares_rate;

    BlockSubmission* block_submission;

    ondemand::parser reqParser;
    ondemand::parser httpParser;

    RedisManager redis_manager;
    // DifficultyManager* diff_manager;

   //  std::deque<std::time_t> block_timestamps;
   // in ms
    std::time_t round_start_timestamp = 0;
    
    std::time_t mature_timestamp = 0;
    std::time_t last_block_timestamp = 0;

    std::mutex clients_mutex;

    static JobManager job_manager;

    std::vector<StratumClient*> clients;
    std::deque<Job*> jobs;

    void Listen();
    void HandleSocket(int sockfd);
    void HandleReq(StratumClient* cli, char buffer[], int reqSize);
    void HandleBlockNotify(ondemand::array& params);
    void HandleWalletNotify(ondemand::array& params);

    void HandleSubscribe(StratumClient* cli, int id, ondemand::array& params);
    void HandleAuthorize(StratumClient* cli, int id, ondemand::array& params);
    void HandleSubmit(StratumClient* cli, int id, ondemand::array& params);

    void HandleShare(StratumClient* cli, int id, const Share& share);
    void SendReject(StratumClient* cli, int id, int error, const char* msg);
    void SendAccept(StratumClient* cli, int id);
    bool SubmitBlock(const char* blockHex, int blockHexLen);

    void UpdateDifficulty(StratumClient* cli);
    void AdjustDifficulty(StratumClient* cli, std::time_t curTime);

    void BroadcastJob(StratumClient* cli, Job* job);

    Job* GetJobById(std::string_view id);
};
#endif