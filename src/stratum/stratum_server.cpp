#include "stratum_server.hpp"

StratumServer::StratumServer(CoinConfig cnfg)
    : coin_config(cnfg), reqParser(REQ_BUFF_SIZE)
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

    job_count = redis_manager->GetJobCount();

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
#if POOL_COIN == COIN_VRSCTEST
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

    auto val = ondemand::array();
    HandleBlockNotify(val);

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
        new StratumClient(conn_fd, time(0), this->coin_config.default_diff);
    clients_mutex.lock();
    this->clients.push_back(client);
    clients_mutex.unlock();

    // simdjson requires some extra bytes
    char buffer[REQ_BUFF_SIZE + SIMDJSON_PADDING];
    char *reqEnd = nullptr;
    int total = 0, res = 0;

    while (true)
    {
        do
        {
            res = recv(conn_fd, buffer + total, REQ_BUFF_SIZE - total - 1, 0);

            if (res <= 0) break;
            total += res;
            buffer[total] = '\0';
        } while ((reqEnd = std::strchr(buffer, '\n')) == NULL);
        // reqEnd += 1;

        if (res <= 0)
        {
            // miner disconnected
            std::cout << "client disconnected. res: " << res << std::endl;
            close(client->GetSock());
            clients_mutex.lock();
            clients.erase(std::find(clients.begin(), clients.end(), client));
            clients_mutex.unlock();
            break;
        }

        // std::cout << "buffer: " << buffer << std::endl;
        // there can be multiple messages
        
        do
        {
            int reqLen = reqEnd - buffer;
            int nextReqLen = total - reqLen - 1;
            std::cout << "len: " << reqLen << std::endl;
            std::cout << "total: " << total << std::endl;
            // without \n
            buffer[reqLen] = '\0';
            HandleReq(client, buffer, reqLen);
            // after the \n
            std::memcpy(buffer, reqEnd + 1, nextReqLen);
            total = nextReqLen;
            buffer[nextReqLen] = '\0';
        } while ((reqEnd = std::strchr(buffer, '\n')) != NULL);
    }
}

void StratumServer::HandleReq(StratumClient *cli, char buffer[], int reqSize)
{
    // std::cout << buffer << std::endl;
    int id = 0;
    std::string_view method;
    ondemand::array params;

    auto start = std::chrono::steady_clock::now();


    try
    {
    ondemand::document doc =
        reqParser.iterate(buffer, reqSize, reqSize + SIMDJSON_PADDING);
    ondemand::object req = doc.get_object();
    id = req["id"].get_int64();
    method = req["method"].get_string();
    params = req["params"].get_array();
    }
    catch (simdjson::simdjson_error err)
    {
        std::cerr << "Req json parse error: " << err.what() << std::endl;
    }

    auto end = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    std::cout << "req parse took: " << dur << "micro seconds." << std::endl;

    // if (!strcmp(method, "mining.subscribe"))
    if (method == "mining.subscribe")
        HandleSubscribe(cli, id, params);
    else if (method == "mining.authorize")
        HandleAuthorize(cli, id, params);
    else if (method == "mining.submit")
        HandleSubmit(cli, id, params);
    else if (method == "mining.block_notify")
        HandleBlockNotify(params);
    else if (method == "mining.wallet_notify")
        HandleWalletNotify(params);
    else
        std::cerr << "Unknown request method: " << method << std::endl;
}

void StratumServer::HandleBlockNotify(ondemand::array &params)
{
    // first of all clear jobs to insure we are not paying for stale shares
    this->jobs.clear();
    auto start = std::chrono::steady_clock::now();

    char reqParams[] = "";
    std::vector<char> resBody;
    int resCode = SendRpcReq(resBody, 1, "getblocktemplate", reqParams, 0);
    while (resCode != 200)
    {
        resCode = SendRpcReq(resBody, 1, "getblocktemplate", reqParams,
                             sizeof(reqParams));
        std::cerr << "Block update socket error: Failed to get blocktemplate, "
                     "http code: "
                  << resCode << ", retrying..." << std::endl;

        std::this_thread::sleep_for(5ms);
    }

    Job *job;

    try
    {
        // std::cout << resBody.data() << std::endl;

        ondemand::document doc = httpParser.iterate(
            resBody.data(), resBody.size() - SIMDJSON_PADDING, resBody.size());

        ondemand::object res = doc["result"].get_object();

        // this must be in the order they appear in the result for simdjson
        int version = res["version"].get_int64();
        std::string_view prevBlockHash = res["previousblockhash"].get_string();
#if POOL_COIN == COIN_VRSCTEST
        std::string_view finalSRoot = res["finalsaplingroothash"].get_string();
        std::string_view solution = res["solution"].get_string();
#endif
        ondemand::array txs = res["transactions"].get_array();
        int value = res["coinbasetxn"]["coinbasevalue"].get_int64();
        uint32_t curtime = res["curtime"].get_int64();
        std::string_view bits = res["bits"].get_string();
        uint32_t height = res["height"].get_int64();

        std::vector<std::vector<unsigned char>> transactions(1);
        transactions[0] = GetCoinbaseTx(value, curtime, height);

        auto end = std::chrono::steady_clock::now();

#if POOL_COIN == COIN_VRSCTEST

        // reverse all fields to header encoding
        char prevBlockRev[64 + 1];
        char finalSRootRev[64 + 1];
        char bitsRev[8 + 1];
        char sol144[144 + 1];

        ReverseHex(prevBlockHash.data(), 64, prevBlockRev);
        ReverseHex(finalSRoot.data(), 64, finalSRootRev);
        ReverseHex(bits.data(), 8, bitsRev);
        memcpy(sol144, solution.data(), 144);

        prevBlockRev[64] = '\0';
        finalSRootRev[64] = '\0';
        bitsRev[8] = '\0';
        sol144[144] = '\0';

        uint32_t versionRev = bswap_32(version);
        uint32_t curtimeRev = bswap_32(curtime);

        job =
            new VerusJob(this->job_count, transactions, true, version,
                         prevBlockRev, curtime, bitsRev, finalSRootRev, sol144);
#endif

        std::cout << "Generated job: " << job->GetId() << " in " << std::dec
                  << std::chrono::duration_cast<std::chrono::microseconds>(
                         end - start)
                         .count()
                  << "us" << std::endl;
        // block->AddTransaction(coinbaseTx);
        // for (ondemand::array::Constondemand::arrayIterator itr =
        // txs.begin();
        // itr
        // !=
        // txs.end();
        // itr++)
        //     block->AddTransaction((*itr)["data"].GetString());

        this->redis_manager->SetJobCount(++this->job_count);

        // std::cout << "Block update: " << height << std::endl;
        std::cout << "Difficulty target: " << std::fixed << job->GetTargetDiff()
                  << std::endl;
        std::cout << "Broadcasted job: " << job->GetId() << std::endl;
    }
    catch (simdjson::simdjson_error err)
    {
        std::cerr << "getblocktemplate json parse error: " << err.what()
                  << std::endl;
    }

    jobs.push_back(job);
    this->BroadcastJob(job);
}

void StratumServer::HandleWalletNotify(ondemand::array &params)
{
    std::string_view txId = (*params.begin()).get_string();

    std::cout << "Received transaction: " << txId << std::endl;

    std::vector<char> resBody;
    char rpcParams[64];
    int len = snprintf(rpcParams, sizeof(rpcParams), "\"%s\",1",
                       std::string(txId).c_str());
    rpcParams[len] = 0;

    int resCode = SendRpcReq(resBody, 1, "getrawtransaction", rpcParams, len);

    if (resCode != 200)
    {
        std::cerr << "CheckAcceptedBlock error: Failed to getrawtransaction, "
                     "http code: "
                  << resCode << std::endl;
        // return CheckAcceptedBlock(height);
    }

    ondemand::document doc =
        httpParser.iterate(resBody.data(), resBody.size() - 32, resBody.size());

    std::cout << "f: " << doc.count_fields() << std::endl;
    // ondemand::object res = doc["result"].get_object();
    // std::string_view validationType = res["validationtype"].get_string();

    // GenericObject res = doc["result"].GetObject();
    // GenericArray txIds = res["tx"].GetArray();
    // std::string validationType = res["validationtype"].GetString();
    // std::string_view coinbaseTxId =
    //     (*res["tx"].get_array().begin()).get_string();

    // // parse the (first) coinbase transaction of the block
    // // and check if it outputs to our pool address
}

// TODO: VERIFY EQUIHASH ON SHARE
// TODO: check if rpc response contains error

void StratumServer::HandleSubscribe(StratumClient *cli, int id,
                                    ondemand::array &params)
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
                       "{\"id\":%d,\"result\":[null,\"%s\"],\"error\":null}\n",
                       id, cli->GetExtraNonce());
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

void StratumServer::HandleAuthorize(StratumClient *cli, int id,
                                    ondemand::array &params)
{
    char response[1024];
    int len = 0, split = 0;
    std::string error = "null", addr, worker, worker_full;

    // worker name format: address.worker_name
    /*worker_full = */  // params.at(0).get_string();
    // split = worker_full.find('.');

    // if (split == std::string::npos)
    // {
    //     error = "wrong worker name format, use: address/id@.worker";
    // }
    // else
    // {
    //     // addr = worker_full.substr(0, split);
    //     // worker = worker_full.substr(split + 1, worker_full.size() - 1);

    //     // std::string params = "\"" + addr + "\"";
    //     // char *res = SendRpcReq(1, "validateaddress", params);
    //     // Document doc(kObjectType);
    //     // doc.Parse(res);

    //     // bool isvalid = doc.HasMember("result") && doc["result"].IsObject()
    //     &&
    //     //                doc["result"].HasMember("isvalid") &&
    //     //                doc["result"]["isvalid"].IsBool() &&
    //     //                doc["result"]["isvalid"] == true;

    //     // if (!isvalid)
    //     //     error =
    //     //         "invalid address or identity, use format:
    //     //         address/id@.worker";

    //     // std::cout << "Address: " << addr << ", worker: " << worker
    //     //           << ", authorized: " << isvalid << std::endl;
    // }

    std::string result = error == "null" ? "true" : "false";

    len = snprintf(response, sizeof(response),
                   "{\"id\":%i,\"result\":%s,\"error\":\"%s\"}\n", id,
                   result.c_str(), error.c_str());

    send(cli->GetSock(), response, len, 0);
}

// https://zips.z.cash/zip-0301#mining-submit
void StratumServer::HandleSubmit(StratumClient *cli, int id,
                                 ondemand::array &params)
{
    auto start = std::chrono::steady_clock::now();
    Share share;
    auto it = params.begin();
    share.worker = (*it).get_string();
    share.jobId = (*++it).get_string();
    share.time = (*++it).get_string();
    share.nonce2 = (*++it).get_string();
    share.solution = (*++it).get_string();

    auto end = std::chrono::steady_clock::now();
    HandleShare(cli, id, share);
    auto duration =
        std::chrono::duration_cast<microseconds>(end - start).count();
    std::cout << "Share processed in " << duration << "microseconds."
              << std::endl;
}

void StratumServer::HandleShare(StratumClient *cli, int id, Share &share)
{
    // auto start = std::chrono::steady_clock::now();
    Job *job = GetJobById(share.jobId);
    std::time_t time = std::time(0);
    cli->SetLastShare(time);

    if (job == nullptr)
    {
        redis_manager->AddShare(share.worker, STALE_SHARE_DIFF, time);
        return RejectShare(cli, id, ShareResult::JOB_NOT_FOUND);
    }

    // TODO: check duplicate

    unsigned char *headerData = job->GetHeaderData(
        share.time, cli->GetExtraNonce(), share.nonce2, share.solution);
    unsigned char hashBuff[32];

    // std::cout << "header: ";
    // for (int i = 0; i < BLOCK_HEADER_SIZE; i++)
    //     std::cout << std::hex << std::setfill('0') << std::setw(2)
    //               << (int)headerData[i];
    // std::cout << std::endl;
    job->GetHash(hashBuff);

    std::vector<unsigned char> v(hashBuff, hashBuff + 32);
    uint256 hash256(v);

    double shareDiff = BitsToDiff(UintToArith256(hash256).GetCompact(false));

    std::cout << "block hash      : " << hash256.GetHex() << std::endl;
    std::cout << "block difficulty: " << job->GetTargetDiff() << std::endl;
    std::cout << "share difficulty: " << shareDiff << std::endl;
    // std::cout << "client target   : " << cli->GetDifficulty() << std::endl;
    // auto end = std::chrono::steady_clock::now();
    // auto duration =
    //     std::chrono::duration_cast<microseconds>(end - start).count();
    // std::cout << "duration x " << duration << "microseconds." << std::endl;
    if (shareDiff >= job->GetTargetDiff())
    {
        std::cout << "Found block solution!" << std::endl;
        int blockSize = job->GetBlockSize();
        char *blockData = new char[blockSize + 3];
        blockData[0] = '\"';
        job->GetBlockHex(blockData + 1);
        blockData[blockSize + 1] = '\"';
        blockData[blockSize + 2] = 0;
        // std::cout << (char *)blockData << std::endl;

        bool submissionGood = SubmitBlock(blockData, blockSize + 3);
        delete[] blockData;
        if (submissionGood)
        {
            std::cout << "Submit block successful." << std::endl;
            // redis_manager->InsertPendingBlock(hash_str);
        }
        else
        {
            std::cerr << "Error submitting block, retrying..." << std::endl;
            // submissionGood = SubmitBlock(blockHex);
        }
    }
    else if (shareDiff < cli->GetDifficulty())
    {
        redis_manager->AddShare(share.worker, INVALID_SHARE_DIFF, time);
        return RejectShare(cli, id, ShareResult::LOW_DIFFICULTY_SHARE);
    }

    char buffer[1024];
    int len = snprintf(buffer, sizeof(buffer),
                       "{\"id\":%d,\"result\":true,\"error\":null}\n", id);
    send(cli->GetSock(), buffer, len, 0);
    redis_manager->AddShare(share.worker, shareDiff, time);
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
    int len =
        snprintf(buffer, sizeof(buffer),
                 "{\"id\":%d,\"result\":null,\"error\":[%d,\"%s\",null]}\n", id,
                 (int)error, errorMessage.c_str());
    send(cli->GetSock(), buffer, len, 0);

    std::cout << "rejected share: " << errorMessage << std::endl;
}

bool StratumServer::SubmitBlock(const char *blockHex, int blockHexLen)
{
    std::vector<char> resultBody;
    int resCode =
        SendRpcReq(resultBody, 1, "submitblock", blockHex, blockHexLen);
    resultBody.resize(resultBody.size() + SIMDJSON_PADDING);

    if (resCode != 200)
    {
        std::cerr << "Failed to send block submission, http code: " << resCode
                  << std::endl;
        return false;
    }

    try
    {
        // ondemand::document doc = parser.iterate(
        //     resultBody.data(), resultBody.size() - SIMDJSON_PADDING,
        //     resultBody.size());

        // ondemand::object res = doc.get_object();
        // ondemand::value errorField = res.find_field("error");

        // if (!errorField.is_null())
        // {
        //     std::cerr << "Block submission rejected, http code: " << resCode
        //               << " rpc error: " << errorField.get_raw_json_string()
        //               << std::endl;
        //     return false;
        // }
    }
    catch (simdjson::simdjson_error err)
    {
        std::cerr << "Submit block response parse error: " << err.what()
                  << std::endl;
        // std::cerr << "response: " <<
        return false;
    }

    return true;
}

void StratumServer::UpdateDifficulty(StratumClient *cli)
{
    uint32_t diffBits = DiffToBits(cli->GetDifficulty());
    uint256 diff256;
    arith_uint256 arith256 = UintToArith256(diff256).SetCompact(diffBits);

    char request[1024];
    int len = snprintf(
        request, sizeof(request),
        "{\"id\":null,\"method\":\"mining.set_target\",\"params\":[\"%s\"]}\n",
        arith256.GetHex().c_str());

    send(cli->GetSock(), request, len, 0);
}

void StratumServer::BroadcastJob(Job *job)
{
    clients_mutex.lock();
    std::vector<StratumClient *>::iterator it;
    for (it = clients.begin(); it != clients.end(); it++)
        send((*it)->GetSock(), job->GetNotifyBuff(), job->GetNotifyBuffSize(),
             0);
    clients_mutex.unlock();
}

void StratumServer::BroadcastJob(StratumClient *cli, Job *job)
{
    send(cli->GetSock(), job->GetNotifyBuff(), job->GetNotifyBuffSize(), 0);
}

std::vector<unsigned char> StratumServer::GetCoinbaseTx(int64_t value,
                                                        uint32_t curtime,
                                                        uint32_t height)
{
    unsigned char prevTxIn[32] = {0};
    std::vector<unsigned char> signature(1 + 4);
    signature[0] = 0x03;  // var int

    memcpy(signature.data() + 1, &height, 4);

    // todo: make txVersionGroup updateable without reset
    uint32_t txVersionGroup = 0x892f2085;
    VerusTransaction coinbaseTx(4, curtime, true, txVersionGroup);
    coinbaseTx.AddInput(prevTxIn, UINT32_MAX, signature, UINT32_MAX);
    coinbaseTx.AddStdOutput(this->coin_config.pool_addr.c_str(),
                            value);  // TODO: add i address check
    coinbaseTx.AddTestnetCoinbaseOutput();
    return coinbaseTx.GetBytes();
}

Job *StratumServer::GetJobById(std::string_view id)
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

int StratumServer::SendRpcReq(std::vector<char> &result, int id,
                              const char *method, const char *params,
                              int paramsLen)
{
    for (DaemonRpc *rpc : this->rpcs)
    {
        int res = rpc->SendRequest(result, id, method, params, paramsLen);
        if (res != -1) return res;
    }

    return -2;
}