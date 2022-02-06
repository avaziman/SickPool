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

    redis_manager = new RedisManager(cnfg.symbol, cnfg.redis_host);

    job_count = redis_manager->GetJobId();

    std::cout << "Job count: " << job_count << std::endl;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

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
    if (coinConfig.algo == "verushash") CVerusHashV2::init();
}

StratumServer::~StratumServer()
{
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
            std::cerr << "Invalid connecting socket accepted. Skipping..."
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
        int res = 0;
        do
        {
            res = recv(conn_fd, buffer + total, sizeof(buffer) - total, 0);
            if (res > 0)
                total += res;
            else
                break;
        } while (buffer[total - 1] != '\n');

        if (res == 0)
        {
            clients.erase(std::find(clients.begin(), clients.end(), client));
            break;  // miner disconnected
        }
        else if (res == /*SOCKET_ERROR*/ -1)
            break;

        buffer[total - 1] = '\0';
        // make sure the request is a json object.
        // getwork uses HTTP so this will ignore it
        if (buffer[0] != '{' || buffer[total - 2] != '}')
        {
            // std::cout << "received non json request: " << buffer <<
            // std::endl;
            continue;
        }

        this->HandleReq(client, buffer);
    }
}

void StratumServer::HandleReq(StratumClient *cli, char buffer[])
{
    // std::cout << buffer << std::endl;
    int id = 0;
    std::string method;
    Value params(kArrayType);

    Document req(kObjectType);
    req.Parse(buffer);

    if (req.HasMember("id") && req["id"].IsInt()) id = req["id"].GetInt();

    method = req["method"].GetString();
    params = req["params"].GetArray();

    if (method == "mining.subscribe")
        HandleSubscribe(cli, id, params);
    else if (method == "mining.authorize")
        HandleAuthorize(cli, id, params);
    else if (method == "mining.update_block")
        HandleBlockUpdate(params);
    else if (method == "mining.submit")
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
    GenericArray txs = res["transactions"].GetArray();
    uint32_t height = res["height"].GetInt();
    int64_t coinbaseValue = res["coinbasetxn"]["coinbasevalue"].GetInt64();
    uint32_t curtime = res["curtime"].GetInt();

    std::string bits = res["bits"].GetString();
    std::string heightHex = ToHex(height);

    Block *block;
    BlockHeader *bh;

    if (coinConfig.name == "verus")
    {
        bh = new VerusBlockHeader(
            res["version"].GetInt(),
            ReverseHex(res["previousblockhash"].GetString()), curtime,
            ReverseHex(bits),
            ReverseHex(res["finalsaplingroothash"].GetString()),
            res["solution"].GetString());
        block = new Block(bh, HashWrapper::SHA256, HashWrapper::VerushashV2b2);
    }

    std::string coinbaseTx =
        GetCoinbaseTx(coinConfig.pool_addr, coinbaseValue, curtime, height);

    block->AddTransaction(coinbaseTx);
    // for (Value::ConstValueIterator itr = txs.begin(); itr != txs.end();
    // itr++)
    //     block->AddTransaction((*itr)["data"].GetString());

    this->redis_manager->SetJobId(++this->job_count);

    Job *job = new VerusJob(this->job_count, *block);
    jobs.push_back(job);
    this->BroadcastJob(job);
    this->CheckAcceptedBlock(height);

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
    params = std::string("\"") + txIds[0].GetString() +
             "\",1";  // 1 = verboose (json)

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

    std::ostringstream ss;
    ss << "{\"id\":" << id << ",\"result\":["
       << "null"
       << ",\"" << cli->GetExtraNonce() << "\"]}\n";
    std::string response = ss.str();
    // std::cout << response << std::endl;
    send(cli->GetSock(), response.c_str(), response.size(), 0);
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
    Job *job = GetJobById(share.jobId);

    if (job == nullptr){
        redis_manager->AddShare(share.worker, STALE_SHARE_DIFF);
        return RejectShare(cli, id, ShareResult::JOB_NOT_FOUND);
    } 

    // TODO: check duplicate

    std::string nonce = cli->GetExtraNonce() + share.nonce2;
    nonce += share.solution;

    std::string headerHex =
        job->Header()->GetHex(share.time, job->GetMerkleRoot(), nonce);

    std::string hash_str = ReverseHex(job->GetHash(headerHex));
    uint256 hash256 = uint256S(hash_str.c_str());

    double shareDiff =
        DifficultyFromBits(UintToArith256(hash256).GetCompact(false));

    // std::cout << "header hex: " << headerHex << std::endl;
    std::cout << "block hash      : " << hash_str << std::endl;
    std::cout << "block difficulty: " << job->GetTargetDiff() << std::endl;
    std::cout << "share difficulty: " << shareDiff << std::endl;
    // std::cout << "client target   : " << cli->GetDifficulty() << std::endl;

    if (shareDiff < cli->GetDifficulty()){
        redis_manager->AddShare(share.worker, INVALID_SHARE_DIFF);
        return RejectShare(cli, id, ShareResult::LOW_DIFFICULTY_SHARE);
    }
    else if (shareDiff >= job->GetTargetDiff())
    {
        std::cout << "found block solution!!!" << std::endl;
        std::string blockHex = job->GetHex(headerHex);

        bool submissionGood = SubmitBlock(blockHex);
        if (submissionGood)
        {
            std::cout << "Submit block successful." << std::endl;
            redis_manager->InsertPendingBlock(hash_str);
        }
        else
        {
            std::cerr << "Error submitting block, retrying..." << std::endl;
            submissionGood = SubmitBlock(blockHex);
        }
    }

    redis_manager->AddShare(share.worker, shareDiff);
    std::cout << "share accepted" << std::endl;
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
    int len = sprintf(
        buffer,
        "{\"id\": %d, \"result\": null, \"error\": [%d, \"%s\", null]}\n", id,
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
    // TODO: format diff
    char request[1024];
    int len = snprintf(
        request, sizeof(request),
        "{\"id\": null, \"method\": \"mining.set_target\", \"params\": "
        "[\"0000004000000000000000000000000000000000000000000000000000000000\"]"
        "}\n");
    // cli->GetDifficulty().c_str());
    // "0000000400000000000000000000000000000000000000000000000000000000");

    send(cli->GetSock(), request, len, 0);
}

void StratumServer::BroadcastJob(Job *job)
{
    char message[1024];
    int len = job->GetNotifyMessage(message, sizeof(message));

    std::vector<StratumClient *>::iterator it;
    for (it = clients.begin(); it != clients.end(); it++)
        send((*it)->GetSock(), message, len, 0);
}

void StratumServer::BroadcastJob(StratumClient *cli, Job *job)
{
    char message[1024];
    int len = job->GetNotifyMessage(message, sizeof(message));

    send(cli->GetSock(), message, len, 0);
}

std::string StratumServer::GetCoinbaseTx(std::string addr, int64_t value,
                                         uint32_t curtime, uint32_t height)
{
    std::string versionGroup = "892f2085";
    std::string heightHex = ToHex(height);

    VerusTransaction coinbaseTx(4, curtime, true, versionGroup);
    coinbaseTx.AddInput(std::string(64, '0'), UINT32_MAX, "03" + heightHex,
                        UINT32_MAX);
    coinbaseTx.AddStdOutput(addr, value);  // TODO: add i address check
    coinbaseTx.AddTestnetCoinbaseOutput();
    return coinbaseTx.GetHex();
}

Job *StratumServer::GetJobById(std::string id)
{
    for (auto it = this->jobs.begin(); it != this->jobs.end(); it++)
    {
        if (id == (*it)->GetId())
        {
            return (*it);
            break;
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