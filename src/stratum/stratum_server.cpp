#include "stratum_server.hpp"

StratumServer::StratumServer(CoinConfig cnfg) : coinConfig(cnfg)
{
    for (int i = 0; i < 4; i++)
    {
        if (cnfg.rpcs[i].host.size() != 0)
        {
            this->rpcs.push_back(
                new DaemonRpc(cnfg.rpcs[i].host, cnfg.rpcs[i].auth));
        }
    }

    // redis_manager = new RedisManager(cnfg.symbol, cnfg.redis_host);

    job_count = 0;
    // job_count = redis_manager->GetJobId();

    std::cout << "Job count: " << job_count << std::endl;

    int optval = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) !=
        0)
        throw std::runtime_error("Failed to set stratum socket options");

    if (sockfd == -1)
        throw std::runtime_error("Failed to create stratum socket");

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(cnfg.stratum_port);

    if (bind(sockfd, (const sockaddr *)&addr, sizeof(addr)) != 0)
    {
        throw std::runtime_error("Failed to bind to port " +
                                 std::to_string(cnfg.stratum_port));
    }

    // init hash functions if needed
#if POOL_COIN == COIN_VRSC
    HashWrapper::InitVerusHash();
#endif
}

StratumServer::~StratumServer()
{
    for (StratumClient *cli : this->clients) close(cli->GetSock());
    close(this->sockfd);
    for (DaemonRpc *rpc : this->rpcs) delete rpc;
}

void StratumServer::StartListening()
{
    if (listen(this->sockfd, 1024) != 0)
        throw std::runtime_error(
            "Stratum server failed to enter listenning state.");

    HandleBlockUpdate(Value().SetNull());

    std::thread(&StratumServer::Listen, this).join();
}

void StratumServer::Listen()
{
    std::cout << "Started listenning on port: " << ntohs(this->addr.sin_port)
              << std::endl;

    while (true)
    {
        int addr_len;
        int conn_fd;
        sockaddr_in conn_addr;
        conn_fd = accept(sockfd, (sockaddr *)&addr, (socklen_t *)&addr_len);
        if (conn_fd <= 0)
        {
            std::cerr << "Invalid connecting socket accepted. Ignoring..."
                      << std::endl;
            continue;
        };

        std::thread(&StratumServer::HandleSocket, this, conn_fd).detach();
    }
}

void StratumServer::HandleSocket(int conn_fd)
{
    StratumClient *client =
        new StratumClient(conn_fd, time(NULL), this->coinConfig.default_diff);
    this->clients.push_back(client);

    while (true)
    {
        char buffer[1024 * 4];
        int total = 0;
        int res = -1;
        do
        {
            res = recv(conn_fd, buffer + total, sizeof(buffer) - total - 1, 0);
            std::cout << res << std::endl;
            std::cout << "char->" << (int)buffer[total - 1] << std::endl;
            std::cout << buffer << std::endl;
            if (res > 0)
                total += res;
            else
                break;
        } while (buffer[total - 1] != '\n');

        if (res == 0)
        {
            // miner disconnected
            std::cout << "client disconnected." << std::endl;
            close(client->GetSock());
            clients.erase(std::find(clients.begin(), clients.end(), client));
            break;
        }

        buffer[total - 1] = '\0';

        std::cout << buffer << std::endl;
        // std::cout << ++this->job_count << std::endl;
        // std::thread(&StratumServer::HandleReq, this, client,
        // buffer).detach();
        HandleReq(client, buffer);
    }
}

void StratumServer::HandleReq(StratumClient *cli, char buffer[])
{
    // std::cout << buffer << std::endl;
    int id = 0;

    auto start = std::chrono::steady_clock::now();

    Value params(kArrayType);

    Document req(kObjectType);
    req.Parse(buffer);

    auto member = req.FindMember("id");
    if (member == req.MemberEnd())
        return;
    else if (member->value.IsInt())
        id = member->value.GetInt();

    member = req.FindMember("method");
    if (member == req.MemberEnd() || !member->value.IsString()) return;
    const char *method = member->value.GetString();

    member = req.FindMember("params");
    if (member == req.MemberEnd() || !member->value.IsArray()) return;
    params = member->value;

    auto end = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    std::cout << "req parse took: " << dur << "micro seconds." << std::endl;

    if (!strcmp(method, "mining.subscribe"))
        HandleSubscribe(cli, id, params);
    else if (!strcmp(method, "mining.authorize"))
        HandleAuthorize(cli, id, params);
    else if (!strcmp(method, "mining.update_block"))
        HandleBlockUpdate(params);
    else if (!strcmp(method, "mining.submit"))
        HandleSubmit(cli, id, params);
}

void StratumServer::HandleBlockUpdate(Value &params)
{
    char *buffer = SendRpcReq(1, "getblocktemplate");
    if (buffer == nullptr)
    {
        std::cerr << "Block update socket error: Failed to get blocktemplate "
                  << std::endl;
        return HandleBlockUpdate(params);
    }

    Document blockT(kObjectType);
    blockT.Parse(buffer);
    delete[] buffer;

    GenericObject res = blockT["result"].GetObject();
    const char *prevBlockHash = res["previousblockhash"].GetString();
    const char *bits = res["bits"].GetString();
    GenericArray txs = res["transactions"].GetArray();
    uint32_t height = res["height"].GetInt();
    int64_t coinbaseValue = res["coinbasetxn"]["coinbasevalue"].GetInt64();
    uint32_t curtime = res["curtime"].GetInt();

    char verHex[8 + 1];
    char curTimeHex[8 + 1];

    ToHex(verHex, bswap_32(res["version"].GetInt()));
    ToHex(curTimeHex, bswap_32(curtime));
    curTimeHex[8] = verHex[8] = 0;

    std::string heightHex = ToHex(height);

    // Block *block;

    std::vector<std::vector<unsigned char>*> transactions;
    std::vector<unsigned char>* coinbaseTx =
        GetCoinbaseTx(coinbaseValue, curtime, height);

    transactions.push_back(coinbaseTx);

    Job *job;

#if POOL_COIN == COIN_VRSC
    const char *finalSaplingRoot = res["finalsaplingroothash"].GetString();
    const char *solution = res["solution"].GetString();

    job =
        new VerusJob(this->job_count, transactions, true, verHex, prevBlockHash,
                     curTimeHex, bits, finalSaplingRoot, (char *)solution);
#endif

    // block->AddTransaction(coinbaseTx);
    // for (Value::ConstValueIterator itr = txs.begin(); itr != txs.end();
    // itr++)
    //     block->AddTransaction((*itr)["data"].GetString());

    // this->redis_manager->SetJobId(++this->job_count);

    jobs.push_back(job);
    this->BroadcastJob(job);
    // this->CheckAcceptedBlock(height);

    std::cout << "Block update: " << height << std::endl;
    std::cout << "Difficulty target: " << std::fixed << job->GetTargetDiff()
              << std::endl;
    std::cout << "Broadcasted job: " << job->GetId() << std::endl;
}
// TODO: VERIFY EQUIHASH ON SHARE
// TODO: check if rpc response contains error
void StratumServer::CheckAcceptedBlock(uint32_t height)
{
    std::string params = "\"" + std::to_string(height - 1) + "\"";
    char *resStr = SendRpcReq(1, "getblock", params);

    if (resStr == nullptr)
    {
        std::cerr << "CheckAcceptedBlock socket error: Failed to getblock "
                  << std::endl;
        return CheckAcceptedBlock(height);
    }

    Document doc(kObjectType);
    doc.Parse(resStr);
    delete[] resStr;

    GenericObject res = doc["result"].GetObject();
    GenericArray txIds = res["tx"].GetArray();
    std::string validationType = res["validationtype"].GetString();

    // parse the (first) coinbase transaction of the block
    // and check if it outputs to our pool address
    params = std::string("\"") + std::string(txIds[0].GetString()) +
             std::string("\",1");  // 1 = verboose (json)

    resStr = SendRpcReq(1, "getrawtransaction", params);
    Document txDoc(kObjectType);
    txDoc.Parse(resStr);
    delete[] resStr;

    std::string solverAddr =
        txDoc["result"]["vout"][0]["scriptPubKey"]["addresses"][0].GetString();

    if (solverAddr != coinConfig.pool_addr) return;

    if (validationType == "work")
    {
        std::cout << "PoW block accepted!" << std::endl;
    }
    else if (validationType == "stake")
    {
        std::cout << "PoS block accepted!" << std::endl;
    }
}

void StratumServer::HandleSubscribe(StratumClient *cli, int id, Value &params)
{
    // Mining software info format: "SickMiner/6.9"
    // if (params.IsArray() && params[0].IsString())
    //     std::string software_info = params[0].GetString();
    // if (params[1].IsString())
    //     std::string last_session_id = params[1].GetString();
    // if (params[2].IsString()) std::string host = params[2].GetString();
    // if (params[3].IsInt()) int port = params[3].GetInt();

    // REQ
    //{"id": 1, "method": "mining.subscribe", "params": ["MINER_USER_AGENT",
    //"SESSION_ID", "CONNECT_HOST", CONNECT_PORT]} \n

    // RES
    //{"id": 1, "result": ["SESSION_ID", "NONCE_1"], "error": null} \n

    // we don't send session id

    char response[1024];
    int len = snprintf(response, sizeof(response),
             "{\"id\":%d,\"result\":[null,\"%s\"],\"error\":null}\n", id,
             cli->GetExtraNonce());
    // std::cout << response << std::endl;
    send(cli->GetSock(), response, len, 0);
    this->UpdateDifficulty(cli);

    if (jobs.size() == 0)
    {
        std::cerr << "no job to broadcast to connected client." << std::endl;
        return;
    }

    this->BroadcastJob(cli, jobs.back());

    std::cout << "client subscribed, broadcasted latest job." << std::endl;
}

void StratumServer::HandleAuthorize(StratumClient *cli, int id, Value &params)
{
    char response[1024];
    int len = 0, split = 0;
    std::string error = "null", addr, worker, worker_full;

    // worker name format: address.worker_name
    worker_full = params[0].GetString();
    split = worker_full.find('.');

    if (split == std::string::npos)
    {
        error = "wrong worker name format, use: address/id@.worker";
    }
    else
    {
        addr = worker_full.substr(0, split);
        worker = worker_full.substr(split + 1, worker_full.size() - 1);

        std::string params = "\"" + addr + "\"";
        char *res = SendRpcReq(1, "validateaddress", params);
        Document doc(kObjectType);
        doc.Parse(res);

        bool isvalid = doc.HasMember("result") && doc["result"].IsObject() &&
                       doc["result"].HasMember("isvalid") &&
                       doc["result"]["isvalid"].IsBool() &&
                       doc["result"]["isvalid"] == true;

        if (!isvalid)
            error =
                "invalid address or identity, use format: address/id@.worker";

        std::cout << "Address: " << addr << ", worker: " << worker
                  << ", authorized: " << isvalid << std::endl;
    }

    std::string result = error == "null" ? "true" : "false";

    len = snprintf(response, sizeof(response),
                   "{\"id\":%i,\"result\":%s,\"error\":\"%s\"}\n", id,
                   result.c_str(), error.c_str());

    send(cli->GetSock(), response, len, 0);
}

// https://zips.z.cash/zip-0301#mining-submit
void StratumServer::HandleSubmit(StratumClient *cli, int id, Value &params)
{
    Share share;
    share.worker = params[0].GetString();
    share.jobId = params[1].GetString();
    share.time = params[2].GetString();
    share.nonce2 = params[3].GetString();
    share.solution = params[4].GetString();

    auto start = std::chrono::steady_clock::now();
    HandleShare(cli, id, share);
    auto end = std::chrono::steady_clock::now();
    auto duration =
        std::chrono::duration_cast<microseconds>(end - start).count();
    std::cout << "Share processed in " << duration << "microseconds."
              << std::endl;
}

void StratumServer::HandleShare(StratumClient *cli, int id, Share &share)
{
    // auto start = std::chrono::steady_clock::now();
    Job *job = GetJobById(share.jobId);

    if (job == nullptr)
    {
        // redis_manager->AddShare(share.worker, STALE_SHARE_DIFF);
        return RejectShare(cli, id, ShareResult::JOB_NOT_FOUND);
    }

    // TODO: check duplicate

    unsigned char *headerData = job->GetData(share.time, cli->GetExtraNonce(),
                                             share.nonce2, share.solution);
    unsigned char hashBuff[32];

    job->GetHash(hashBuff);

    std::vector<unsigned char> v(hashBuff, hashBuff + 32);
    // uint256 hash256 = uint256S(hashBuff);
    uint256 hash256(v);

    double shareDiff =
        BitsToDiff(UintToArith256(hash256).GetCompact(false));

    // std::cout << "header hex: " << headerHex << std::endl;
    // std::cout << "block hash      : " << hash256.GetHex() << std::endl;
    // std::cout << "block difficulty: " << job->GetTargetDiff() << std::endl;
    // std::cout << "share difficulty: " << shareDiff << std::endl;
    // std::cout << "client target   : " << cli->GetDifficulty() << std::endl;
    // auto end = std::chrono::steady_clock::now();
    // auto duration =
    //     std::chrono::duration_cast<microseconds>(end - start).count();
    // std::cout << "duration x " << duration << "microseconds." << std::endl;

    if (shareDiff >= job->GetTargetDiff())
    {
        // std::cout << "found block solution!!!" << std::endl;
        // std::string blockHex = job->GetHex(headerHex);

        // bool submissionGood = SubmitBlock(blockHex);
        // if (submissionGood)
        // {
        //     std::cout << "Submit block successful." << std::endl;
        //     // redis_manager->InsertPendingBlock(hash_str);
        // }
        // else
        // {
        //     std::cerr << "Error submitting block, retrying..." << std::endl;
        //     submissionGood = SubmitBlock(blockHex);
        // }
    }
    else if (shareDiff < cli->GetDifficulty())
    {
        // redis_manager->AddShare(share.worker, INVALID_SHARE_DIFF);
        return RejectShare(cli, id, ShareResult::LOW_DIFFICULTY_SHARE);
    }
    // redis_manager->AddShare(share.worker, shareDiff);
    // std::cout << "share accepted" << std::endl;
}

void StratumServer::RejectShare(StratumClient *cli, int id, ShareResult error)
{
    std::string errorMessage = "none";
    switch (error)
    {
        case ShareResult::UNKNOWN:
            errorMessage = "unknown";
            break;
        case ShareResult::JOB_NOT_FOUND:
            errorMessage = "job not found";
            break;
        case ShareResult::DUPLICATE_SHARE:
            errorMessage = "duplicate share";
            break;
        case ShareResult::LOW_DIFFICULTY_SHARE:
            errorMessage = "low difficulty share";
            break;
        case ShareResult::UNAUTHORIZED_WORKER:
            errorMessage = "unauthorized worker";
            break;
        case ShareResult::NOT_SUBSCRIBED:
            errorMessage = "not subscribed";
            break;
    }

    char buffer[1024];
    int len =
        snprintf(buffer, sizeof(buffer),
                 "{\"id\":%d,\"result\":null,\"error\":[%d,\"%s\",null]}\n", id,
                 (int)error, errorMessage.c_str());
    send(cli->GetSock(), buffer, len, 0);

    std::cout << "rejected share: " << errorMessage << std::endl;
}

bool StratumServer::SubmitBlock(std::string blockHex)
{
    // std::cout << "block hex: " << hex << std::endl;
    std::string params = "\"" + blockHex + "\"";
    char *resStr = SendRpcReq(1, "submitblock", params);
    if (resStr == nullptr)
    {
        std::cerr << "Failed to send block submission." << std::endl;
        delete[] resStr;
        return false;
    }

    Document res(kObjectType);
    res.Parse(resStr);

    if (res.HasMember("error") && !res["error"].IsNull())
    {
        std::string error = "unknown";
        if (res["error"].HasMember("message"))
            error = res["error"]["message"].GetString();

        std::cerr << "Block submission rejected, rpc error: " << error
                  << std::endl;
        delete[] resStr;
        return false;
    }
    else if (res.HasMember("result") && res["result"].IsNull())
    {
        delete[] resStr;
        return true;
    }

    std::cerr << "Block submission failed, unknown rpc result: " << resStr
              << std::endl;
    delete[] resStr;
    return false;
}

void StratumServer::UpdateDifficulty(StratumClient *cli)
{
    uint32_t diffBits = DiffToBits(cli->GetDifficulty());
    uint256 diff256;
    UintToArith256(diff256).SetCompact(diffBits);

    char request[1024];
    int len =
        snprintf(request, sizeof(request),
                 "{\"id\":null,\"method\":\"mining.set_target\",\"params\":["
                 "\"%s"
                 "0000\"]}\n",
                 diff256.GetHex().c_str());

    send(cli->GetSock(), request, len, 0);
}

void StratumServer::BroadcastJob(Job *job)
{
    std::vector<StratumClient *>::iterator it;
    for (it = clients.begin(); it != clients.end(); it++)
        send((*it)->GetSock(), job->GetNotifyBuff(), job->GetNotifyBuffSize(),
             0);
}

void StratumServer::BroadcastJob(StratumClient *cli, Job *job)
{
    send(cli->GetSock(), job->GetNotifyBuff(), job->GetNotifyBuffSize(), 0);
}

std::vector<unsigned char>* StratumServer::GetCoinbaseTx(int64_t value,
                                                        uint32_t curtime,
                                                        uint32_t height)
{
    unsigned char prevTxIn[32] = {0};
    std::vector<unsigned char> signature;
    const unsigned char sizeOfHeight = 0x03;
    signature.push_back(sizeOfHeight);

    memcpy(signature.data(), &height, 4);

    // todo: make txVersionGroup updateable without reset
    uint32_t txVersionGroup = 0x892f2085;
    VerusTransaction coinbaseTx(4, curtime, true, txVersionGroup);
    coinbaseTx.AddInput(prevTxIn, UINT32_MAX, signature,
                        UINT32_MAX);
    coinbaseTx.AddStdOutput(this->coinConfig.pool_addr.c_str(), value);  // TODO: add i address check
    coinbaseTx.AddTestnetCoinbaseOutput();
    return coinbaseTx.GetBytes();
}

Job *StratumServer::GetJobById(std::string id)
{
    for (auto it = this->jobs.begin(); it != this->jobs.end(); it++)
    {
        if (id == (*it)->GetId())
        {
            return (*it);
        }
    }
    return nullptr;
}

char *StratumServer::SendRpcReq(int id, std::string method, std::string params)
{
    for (DaemonRpc *rpc : this->rpcs)
    {
        char *res = rpc->SendRequest(id, method, params);
        if (res == nullptr) break;
        return res;
    }

    return nullptr;
}