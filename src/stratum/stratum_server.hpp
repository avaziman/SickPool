#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_
#define VERUS_MAX_BLOCK_SIZE (1024 * 1024 * 2)

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>  // inet_ntop, inet_pton
#else
#include <sys/socket.h>
#endif

#include <rapidjson/document.h>

#include <chrono>
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
#include "job.hpp"
#include "share.hpp"
#include "share_error.hpp"
#include "stratum_client.hpp"
#include "verus_job.hpp"

using namespace rapidjson;
using namespace std::chrono;

class StratumServer
{
   public:
    StratumServer(CoinConfig config);
    void StartListening();
    int sockfd;

   private:
    CoinConfig coinConfig;
    sockaddr_in addr;

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
    void RejectShare(StratumClient* cli, int id, ShareError error);
    bool SubmitBlock(std::string blockHex);

    void UpdateDifficulty(StratumClient* cli);

    void BroadcastJob(Job* job);
    void BroadcastJob(StratumClient* cli, Job* job);

    void CheckAcceptedBlock(uint32_t height);

    std::string GetCoinbaseTx(std::string addr, int64_t value, uint32_t curtime,
                              uint32_t height);

    char* SendRpcReq(int id, std::string method, std::string params = "");
};
#endif