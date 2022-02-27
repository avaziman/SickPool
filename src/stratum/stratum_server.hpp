#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#include <rapidjson/document.h>
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
#include "../sock_addr.hpp"
#include "difficulty_manager.hpp"
#include "byteswap.h"
#include "job.hpp"
#include "redis_manager.hpp"
#include "share.hpp"
#include "share_result.hpp"
#include "stratum_client.hpp"
#include "verus_job.hpp"

#define REQ_BUFF_SIZE 1024 * 16
#define SOCK_TIMEOUT 5;

// how we store stale and invalid shares in database
#define STALE_SHARE_DIFF -1
#define INVALID_SHARE_DIFF -2

using namespace sw::redis;

using namespace rapidjson;
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

    RedisManager* redis_manager;

    std::mutex clients_mutex;
    std::vector<DaemonRpc*> rpcs;
    std::vector<StratumClient*> clients;
    std::vector<Job*> jobs;

    void Listen();
    void HandleSocket(int sockfd);
    void HandleReq(StratumClient* cli, char buffer[]);
    void HandleBlockUpdate(Value& params);

    void HandleSubscribe(StratumClient* cli, int id, Value& params);
    void HandleAuthorize(StratumClient* cli, int id, Value& params);
    void HandleSubmit(StratumClient* cli, int id, Value& params);

    void HandleShare(StratumClient* cli, int id, Share& share);
    void RejectShare(StratumClient* cli, int id, ShareResult error);
    bool SubmitBlock(std::string blockHex);

    void UpdateDifficulty(StratumClient* cli);

    void BroadcastJob(Job* job);
    void BroadcastJob(StratumClient* cli, Job* job);

    void GetNextReq(int sockfd, int received, char* buffer);

    Job* GetJobById(std::string id);

    void CheckAcceptedBlock(uint32_t height);

    std::vector<unsigned char> GetCoinbaseTx(int64_t value, uint32_t curtime,
                                             uint32_t height);

    int SendRpcReq(std::vector<char>& result, int id, const char* method,
                   std::string& params);
};
#endif