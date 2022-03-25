#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#include <simdjson.h>
#include <sw/redis++/redis++.h>
#include <sys/socket.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <vector>

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
#include "byteswap.h"
#include "job.hpp"
#include "redis_manager.hpp"
#include "share.hpp"
#include "share_processor.hpp"
#include "share_result.hpp"
#include "stratum_client.hpp"
#include "verus_job.hpp"

#define REQ_BUFF_SIZE 4000//(1024 * 32)
#define SOCK_TIMEOUT 5;

using namespace sw::redis;

using namespace simdjson;
using namespace std::chrono;

class StratumServer
{
   public:
    StratumServer(CoinConfig config);
    ~StratumServer();
    void StartListening();

   private:
    int sockfd;
    CoinConfig coin_config;
    sockaddr_in addr;
    uint32_t job_count;
    double target_shares_rate;

    ondemand::parser reqParser;
    ondemand::parser httpParser;

    RedisManager redis_manager;
    // DifficultyManager* diff_manager;

    std::mutex clients_mutex;
    std::vector<DaemonRpc*> rpcs;
    std::vector<StratumClient*> clients;
    std::vector<Job*> jobs;

    void Listen();
    void HandleSocket(int sockfd);
    void HandleReq(StratumClient* cli, char buffer[], int reqSize);
    void HandleBlockNotify(ondemand::array& params);
    void HandleWalletNotify(ondemand::array& params);

    void HandleSubscribe(StratumClient* cli, int id, ondemand::array& params);
    void HandleAuthorize(StratumClient* cli, int id, ondemand::array& params);
    void HandleSubmit(StratumClient* cli, int id, ondemand::array& params);

    void HandleShare(StratumClient* cli, int id, const Share& share);
    void RejectShare(StratumClient* cli, int id, int error, const char* msg);
    void AcceptShare(StratumClient* cli, int id);
    bool SubmitBlock(const char* blockHex, int blockHexLen);

    void UpdateDifficulty(StratumClient* cli);
    void AdjustDifficulty(StratumClient* cli, std::time_t curTime);

    void BroadcastJob(StratumClient* cli, Job* job);

    void GetNextReq(int sockfd, int received, char* buffer);

    Job* GetJobById(std::string_view id);

    std::vector<unsigned char> GetCoinbaseTx(int64_t value, uint32_t curtime,
                                             uint32_t height);

    int SendRpcReq(std::vector<char>& result, int id, const char* method,
                   const char* params, int paramsLen);
};
#endif