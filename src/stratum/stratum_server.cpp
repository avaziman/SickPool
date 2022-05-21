#include "stratum_server.hpp"

std::mutex StratumServer::rpc_mutex;
CoinConfig StratumServer::coin_config;
std::vector<DaemonRpc *> StratumServer::rpcs;
JobManager StratumServer::job_manager;

StratumServer::StratumServer()
    : reqParser(REQ_BUFF_SIZE),
      httpParser(MAX_HTTP_REQ_SIZE),
      redis_manager(coin_config.redis_host),
      block_submissions()
{
    simdjson::error_code error =
        reqParser.allocate(REQ_BUFF_SIZE, MAX_HTTP_JSON_DEPTH);

    if (error != simdjson::SUCCESS)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Failed to allocate request parser buffer: %d -> %s", error,
                    simdjson::error_message(error));
        exit(EXIT_FAILURE);
    }

    error = httpParser.allocate(MAX_HTTP_REQ_SIZE, MAX_HTTP_JSON_DEPTH);
    if (error != simdjson::SUCCESS)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Failed to allocate http parser buffer: %d -> %s", error,
                    simdjson::error_message(error));
        exit(EXIT_FAILURE);
    }

    // reload last round start
    // round_start_pow = redis_manager.GetLastRoundTimePow();
    // (db was empty)
    // if (!round_start_pow) round_start_pow = GetCurrentTimeMs();

    // round_start_pos = redis_manager.GetLastRoundTimePos();

    // Logger::Log(LogType::Info, LogField::Stratum,
    //             "Last PoW round start: %" PRId64, round_start_pow);

    // coin_config = cnfg;
    // for (int i = 0; i < cnfg.rpcs.size(); i++)
    // {
    //     this->rpcs.push_back(new DaemonRpc(std::string(cnfg.rpcs[i].host),
    //                                        std::string(cnfg.rpcs[i].auth)));
    // }

    int optval = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) !=
        0)
        throw std::runtime_error("Failed to set stratum socket options");

    if (sockfd == -1)
        throw std::runtime_error("Failed to create stratum socket");

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(coin_config.stratum_port);

    if (bind(sockfd, (const sockaddr *)&addr, sizeof(addr)) != 0)
    {
        throw std::runtime_error("Failed to bind to port " +
                                 std::to_string(coin_config.stratum_port));
    }

    // init hash functions if needed
    HashWrapper::InitSHA256();
#if POOL_COIN <= COIN_VRSC
    HashWrapper::InitVerusHash();
#endif

    redis_manager.ResetWorkerCount();
    // std::vector<char> c;
    // SendRpcReq(c, 1, "getblockcount", nullptr, 0);
    redis_manager.UpdatePoS(0, GetCurrentTimeMs());
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

    std::thread listeningThread(&StratumServer::Listen, this);
    listeningThread.join();
}

void StratumServer::Listen()
{
    Logger::Log(LogType::Info, LogField::Stratum,
                "Started listenning on port: %d", ntohs(this->addr.sin_port));

    while (true)
    {
        int conn_fd;
        struct sockaddr_in conn_addr;
        socklen_t addr_len = sizeof(conn_addr);
        conn_fd = accept(sockfd, (sockaddr *)&conn_addr, &addr_len);

        std::cout << "tcp client connected, starting new thread..."
                  << std::endl;

        if (conn_fd <= 0)
        {
            std::cerr << "Invalid connecting socket accepted. Ignoring..."
                      << std::endl;
            continue;
        }

        std::thread cliHandler(&StratumServer::HandleSocket, this, conn_fd);
        SetHighPriorityThread(cliHandler);
        cliHandler.detach();
    }
}

void StratumServer::HandleSocket(int conn_fd)
{
    StratumClient *client =
        new StratumClient(conn_fd, time(0), this->coin_config.default_diff);
    clients_mutex.lock();
    this->clients.push_back(client);
    clients_mutex.unlock();

    // simdjson requires some extra bytes, so don't write to the last bytes
    char buffer[REQ_BUFF_SIZE];
    char *reqEnd = nullptr, *reqStart = nullptr;
    int total = 0, reqLen = 0, nextReqLen = 0, recvRes = 0;
    bool isTooBig = false;

    while (true)
    {
        do
        {
            recvRes =
                recv(conn_fd, buffer + total, REQ_BUFF_SIZE_REAL - total, 0);

            if (recvRes <= 0) break;
            total += recvRes;
            buffer[total] = '\0';  // for strchr
            reqEnd = std::strchr(buffer, '}');
        } while (reqEnd == NULL && total < REQ_BUFF_SIZE_REAL);

        // exit loop, miner disconnected
        if (recvRes <= 0)
        {
            close(client->GetSock());
            clients_mutex.lock();
            clients.erase(std::find(clients.begin(), clients.end(), client));
            clients_mutex.unlock();
            Logger::Log(LogType::Info, LogField::Stratum,
                        "client disconnected. res: %d, errno: %d", recvRes,
                        errno);
            redis_manager.DecreaseWorker(client->GetAddress());
            delete client;
            break;
        }
        // std::cout << "buff: " << buffer << std::endl;

        // if (std::strchr(buffer, '\n') == NULL)
        // {
        //     std::cout << "Request too big." << std::endl;
        //     reqStart = &buffer[0];
        //     while (strchr(reqStart, '{') != NULL)
        //     {
        //         reqStart = strchr(buffer, '{');
        //     }

        //     total = total - (&buffer[REQ_BUFF_SIZE] - reqStart);
        //     continue;
        // }

        // there can be multiple messages in 1 recv
        // {1}]\n{2}
        while (reqEnd != NULL)
        {
            reqStart = strchr(buffer, '{');
            reqLen = reqEnd - reqStart + 1;
            HandleReq(client, reqStart, reqLen);

            char *newReqEnd = strchr(reqEnd + 1, '}');
            if (newReqEnd == NULL)
                break;
            else
                reqEnd = newReqEnd;
        }

        nextReqLen = std::strlen(reqEnd + 1);
        std::memmove(buffer, reqEnd + 1, nextReqLen);
        buffer[nextReqLen] = '\0';

        total = nextReqLen;
        // std::cout << "total: " << total << std::endl;
    }
}

void StratumServer::HandleReq(StratumClient *cli, char *buffer, int reqSize)
{
    // std::cout << buffer << std::endl;
    int id = 0;
    std::string_view method;
    ondemand::array params;

    auto start = std::chrono::steady_clock::now();

    // std::cout << "last char -> " << (int)buffer[]
    ondemand::document doc;
    try
    {
        doc = reqParser.iterate(
            padded_string_view(buffer, reqSize, REQ_BUFF_SIZE));

        ondemand::object req = doc.get_object();
        id = req["id"].get_int64();
        method = req["method"].get_string();
        params = req["params"].get_array();
    }
    catch (simdjson::simdjson_error &err)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "JSON parse error: %s\nRequest: %.*s\n", err.what(),
                    reqSize, buffer);
        return;
    }

    auto end = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    // std::cout << "req parse took: " << dur << "micro seconds." << std::endl;

    if (method == "mining.subscribe")
    {
        HandleSubscribe(cli, id, params);
    }
    else if (method == "mining.authorize")
    {
        HandleAuthorize(cli, id, params);
    }
    else if (method == "mining.submit")
    {
        HandleSubmit(cli, id, params);
    }
    else if (method == "stratum.block_notify")
    {
        HandleBlockNotify(params);
    }
    else if (method == "stratum.wallet_notify")
    {
        HandleWalletNotify(params);
    }
    else
        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Unknown request method: %.*s", method.size(),
                    method.data());
}

void StratumServer::HandleBlockNotify(ondemand::array &params)
{
    // TODO: verify the notification
    int64_t curtimeMs = GetCurrentTimeMs();

    auto start = TIME_NOW();

    Job *newJob = job_manager.GetNewJob();

    while (newJob == nullptr)
    {
        newJob = job_manager.GetNewJob();

        Logger::Log(
            LogType::Critical, LogField::Stratum,
            "Block update error: Failed to generate new job! retrying...");

        std::this_thread::sleep_for(250ms);
    }

    std::string idHex = std::string(newJob->GetId());
    jobs.insert({idHex, newJob});
    last_job_id_hex = idHex;

    // save it to process round
    int64_t last_round_start_pow;
    int64_t last_total_effort_pow;

    // round ended
    if (block_submissions.size())
    {
        // last_round_start_pow = round_start_pow;
        // last_total_effort_pow = total_effort_pow;

        // round_start_pow = curtimeMs;
        // total_effort_pow = 0;

        redis_manager.RenameCurrentRound(newJob->GetHeight() - 1);
    }

    clients_mutex.lock();
    for (auto it = clients.begin(); it != clients.end(); it++)
    {
        // (*it)->SetDifficulty(1000000, curtimeMs);
        // (*it)->SetDifficulty(job->GetTargetDiff(), curtimeMs);
        // UpdateDifficulty(*it);
        BroadcastJob(*it, newJob);
    }
    clients_mutex.unlock();

    // for (auto it = clients.begin(); it != clients.end(); it++)
    // {
    //     AdjustDifficulty(*it, curtime);
    // }

    // in case of a crash without shares, we need to update round start time
    // redis_manager.SetNewRoundTime(round_start_pow);

    // double totalEffort = redis_manager.GetTotalEffort(last_round_start);

    BlockSubmission *validSubmission = nullptr;

    for (BlockSubmission *submission : block_submissions)
    {
        unsigned char *prevBlockBin = newJob->GetPrevBlockHash();

        bool blockAdded = !memcmp(
            prevBlockBin, submission->shareRes.HashBytes.data(), HASH_SIZE);

        // only one block submission can be accepted
        if (blockAdded) validSubmission = submission;

        redis_manager.AddBlockSubmission(*submission, blockAdded,
                                         last_total_effort_pow, 0);
    }

    start = TIME_NOW();
    if (block_submissions.size())
    {
        redis_manager.RenameCurrentRound(newJob->GetHeight() - 1);
        if (validSubmission != nullptr)
        {
            redis_manager.ClosePoWRound(
                last_round_start_pow, validSubmission->timeMs,
                newJob->GetBlockReward(), newJob->GetHeight() - 1,
                last_total_effort_pow, coin_config.pow_fee);
            // TODO: fix
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Block added to chain! found by: %s, hash: %.*s",
                        validSubmission->worker.c_str(), HASH_SIZE_HEX,
                        validSubmission->hashHex);
        }
        else
        {
            // 0 reward for invalid blocks
            redis_manager.ClosePoWRound(
                last_round_start_pow, curtimeMs, 0, newJob->GetHeight() - 1,
                last_total_effort_pow, coin_config.pow_fee);
            // TODO: fix

            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Block NOT added to chain %.*s!", HASH_SIZE_HEX,
                        validSubmission->hashHex);
        }

        auto end = TIME_NOW();
        auto elapsed = DIFF_US(end, start);
        Logger::Log(LogType::Debug, LogField::Stratum, "Closed round in %d us",
                    elapsed);

        while (block_submissions.size() > 0)
        {
            delete block_submissions[0];
            block_submissions.pop_front();
        }

        // Logger::Log(LogType::Debug, LogField::Stratum,
        //             "New round start time: %" PRId64, round_start_pow);
    }

    while (jobs.size() > 1)
    {
        // TODO: fix
        //  delete jobs[0];
        //  jobs.erase();
    }

    redis_manager.AddNetworkHr(curtimeMs, newJob->GetTargetDiff());

    std::vector<char> resBody;
    // if (block_timestamps.size() < BLOCK_MATURITY)
    if (mature_timestamp_ms == 0)
    {
        try
        {
            std::string params =
                "\"" +
                std::to_string(newJob->GetHeight() - BLOCK_MATURITY - 1) + "\"";
            int resCode = SendRpcReq(resBody, 1, "getblock", params.c_str(),
                                     params.size());
            ondemand::document doc = httpParser.iterate(
                resBody.data(), resBody.size(), resBody.capacity());

            ondemand::object res = doc["result"].get_object();
            mature_timestamp_ms = res["time"].get_uint64();
            mature_timestamp_ms *= 1000;  // ms accuracy
        }
        catch (simdjson_error &err)
        {
            // it remains zero
            Logger::Log(LogType::Error, LogField::Stratum,
                        "Failed to get mature timestamp error: %s", err.what());
        }
    }
    else
    {
        mature_timestamp_ms =
            mature_timestamp_ms + (curtimeMs - last_block_timestamp_ms);
    }
    redis_manager.SetMatureTimestamp(mature_timestamp_ms);
    last_block_timestamp_ms = curtimeMs;

    Logger::Log(LogType::Info, LogField::Stratum, "Mature timestamp: %" PRId64,
                mature_timestamp_ms);

    Logger::Log(LogType::Info, LogField::JobManager, "Broadcasted new job: #%s",
                newJob->GetId());
    Logger::Log(LogType::Info, LogField::JobManager, "Height: %d",
                newJob->GetHeight());
    Logger::Log(LogType::Info, LogField::JobManager, "Min time: %d",
                newJob->GetMinTime());
    Logger::Log(LogType::Info, LogField::JobManager, "Difficutly: %f",
                newJob->GetTargetDiff());
    // Logger::Log(LogType::Info, LogField::JobManager, "Target: %s",
    //             job->GetTarget()->GetHex().c_str());
    Logger::Log(LogType::Info, LogField::JobManager, "Block reward: %d",
                newJob->GetBlockReward());
    Logger::Log(LogType::Info, LogField::JobManager, "Transaction count: %d",
                newJob->GetTransactionCount());
    Logger::Log(LogType::Info, LogField::JobManager, "Block size: %d",
                newJob->GetBlockSize());
}

void StratumServer::HandleWalletNotify(ondemand::array &params)
{
    std::string_view txId = (*params.begin()).get_string();

    Logger::Log(LogType::Info, LogField::Stratum, "Received TxId: %.*s",
                txId.size(), txId.data());
    // redis_manager.CloseRound(12, 1);
    std::vector<char> resBody;
    char rpcParams[64];
    int len = sprintf(rpcParams, "\"%.*s\",1", (int)txId.size(), txId.data());

    int resCode = SendRpcReq(resBody, 1, "getrawtransaction", rpcParams, len);

    if (resCode != 200)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "CheckAcceptedBlock error: Failed to getrawtransaction, "
                    "http code: %d",
                    resCode);
        return;
    }

    int64_t value;
    std::string_view address, blockHash;

    try
    {
        ondemand::document doc = httpParser.iterate(
            resBody.data(), resBody.size(), resBody.capacity());

        ondemand::object res = doc["result"].get_object();

        ondemand::array vout = res["vout"].get_array();
        auto output1 = (*vout.begin());
        value = output1["valueSat"].get_int64();

        auto addresses = output1["scriptPubKey"]["addresses"];
        address = (*addresses.begin()).get_string();

        if (address != coin_config.pool_addr)
        {
            Logger::Log(LogType::Error, LogField::Stratum,
                        "CheckAcceptedBlock error: Wrong address: %.*s",
                        address.size(), address.data());
            return;
        }

        blockHash = res["blockhash"];
    }
    catch (simdjson_error &err)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "HandleWalletNotify: Failed to parse json, error: %s",
                    err.what());
        return;
    }

    len =
        sprintf(rpcParams, "\"%.*s\"", (int)blockHash.size(), blockHash.data());
    resCode = SendRpcReq(resBody, 1, "getblock", rpcParams, len);

    if (resCode != 200)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "CheckAcceptedBlock error: Failed to getblock, "
                    "http code: %d",
                    resCode);
        return;
    }

    try
    {
        ondemand::document doc = httpParser.iterate(
            resBody.data(), resBody.size(), resBody.capacity());

        ondemand::object res = doc["result"].get_object();
        std::string_view validationType = res["validationtype"];
        if (validationType == "stake")
        {
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Found PoS Block! hash: %.*s", blockHash.size(),
                        blockHash.data());
        }

        Logger::Log(LogType::Info, LogField::Stratum, "validationtype: %.*s",
                    validationType.size(), validationType.data());
    }
    catch (simdjson_error &err)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "HandleWalletNotify (1): Failed to parse json, error: %s",
                    err.what());
        return;
    }

    // parse the (first) coinbase transaction of the block
    // and check if it outputs to our pool address
}
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
    SendRaw(cli->GetSock(), response, len);

    std::cout << "client subscribed!" << std::endl;
}

void StratumServer::HandleAuthorize(StratumClient *cli, int id,
                                    ondemand::array &params)
{
    int split = 0, resCode = 0;
    std::string reqParams;
    std::string_view given_addr, valid_addr, worker, idTag = "null";
    bool isValid = false, isIdentity = false;

    std::vector<char> resultBody;
    ondemand::document doc;
    ondemand::object res;

    // worker name format: address.worker_name
    std::string_view worker_full = (*params.begin()).get_string();
    split = worker_full.find('.');

    if (split == std::string_view::npos)
    {
        SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                   "invalid worker name format, use: address/id@.worker");
        return;
    }
    else if (worker_full.size() > MAX_WORKER_NAME_LEN)
    {
        SendReject(
            cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
            "Worker name too long! (max " str(MAX_WORKER_NAME_LEN) " chars)");
        return;
    }

    given_addr = worker_full.substr(0, split);
    worker = worker_full.substr(split + 1, worker_full.size() - 1);

    bool oldAddress = redis_manager.DoesAddressExist(given_addr, valid_addr);

    if (!oldAddress)
    {
        reqParams = "\"" + std::string(given_addr) + "\"";

        resCode = SendRpcReq(resultBody, 1, "validateaddress",
                             reqParams.c_str(), reqParams.size());
        try
        {
            doc = httpParser.iterate(resultBody.data(), resultBody.size(),
                                     resultBody.capacity());

            res = doc["result"].get_object();

            isValid = res["isvalid"].get_bool();
            if (!isValid)
            {
                SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                           "Invalid address!");
                return;
            }

            valid_addr = res["address"].get_string();
            isIdentity = valid_addr[0] == 'i';
        }
        catch (simdjson_error err)
        {
            SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                       "Server error: Failed to validate address!");
            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Authorize RPC (validateaddress) failed: %s",
                        err.what());
            return;
        }

        if (isIdentity)
        {
            if (given_addr == valid_addr)
            {
                // we were given an identity address, get the id@
                reqParams = "\"" + std::string(valid_addr) + "\"";
                resCode = SendRpcReq(resultBody, 1, "getidentity",
                                     valid_addr.data(), valid_addr.size());

                try
                {
                    doc =
                        httpParser.iterate(resultBody.data(), resultBody.size(),
                                           resultBody.capacity());

                    res = doc["result"].get_object();
                    idTag = res["name"];
                }
                catch (simdjson_error err)
                {
                    SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                               "Server error: Failed to get id!");
                    Logger::Log(LogType::Critical, LogField::Stratum,
                                "Authorize RPC (getidentity) failed: %s",
                                err.what());
                    return;
                }
            }
            else
            {
                // we were given an id@
                idTag = given_addr;
            }
        }
        redis_manager.AddAddress(valid_addr, idTag);
    }

    std::string worker_full_str =
        std::string(std::string(valid_addr) + "." + std::string(worker));
    worker_full = worker_full_str;

    cli->HandleAuthorized(worker_full, valid_addr);
    redis_manager.AddWorker(valid_addr, worker_full, GetDailyTimestamp());

    Logger::Log(LogType::Info, LogField::Stratum,
                "Authorized worker: %.*s, address: %.*s, id: %.*s",
                worker.size(), worker.data(), valid_addr.size(),
                valid_addr.data(), idTag.size(), idTag.data());

    SendAccept(cli, id);
    this->UpdateDifficulty(cli);

    if (jobs.size() == 0)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "no job to broadcast to connected client!");
        return;
    }

    this->BroadcastJob(cli, jobs[last_job_id_hex]);
}

// https://zips.z.cash/zip-0301#mining-submit
void StratumServer::HandleSubmit(StratumClient *cli, int id,
                                 ondemand::array &params)
{
    // parsing takes 0-1 us
    auto start = steady_clock::now();
    Share share;
    try
    {
        auto it = params.begin();
        share.worker = (*it).get_string();
        share.jobId = (*++it).get_string();
        share.time = (*++it).get_string();
        share.nonce2 = (*++it).get_string();
        share.solution = (*++it).get_string();
    }
    catch (simdjson_error &err)
    {
        SendReject(cli, id, (int)ShareCode::UNKNOWN, "invalid params");
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Failed to parse submit: %s", err.what());
        return;
    }

    auto end = steady_clock::now();
    HandleShare(cli, id, share);
    // auto duration = duration_cast<microseconds>(end - start).count();
}

void StratumServer::HandleShare(StratumClient *cli, int id, const Share &share)
{
    int64_t time = GetCurrentTimeMs();

    ShareResult shareRes;

    auto start = TIME_NOW();
    Job *job = jobs[std::string(share.jobId)];

    if (job == nullptr)
    {
        shareRes.Code = ShareCode::JOB_NOT_FOUND;
        shareRes.Message = "Job not found";
        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Received share for unknown job id: %.*s",
                    share.jobId.size(), share.jobId.data());
    }
    else
    {
        shareRes = ShareProcessor::Process(time, *cli, *job, share);
        // shareRes.Code = ShareCode::VALID_BLOCK;
    }
    auto end = TIME_NOW();
    switch (shareRes.Code)
    {
        case ShareCode::VALID_BLOCK:
        {
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Found block solution!");
            int blockSize = job->GetBlockSize();
            char *blockData = new char[blockSize + 3];
            blockData[0] = '\"';
            job->GetBlockHex(cli->GetBlockheaderBuff(), blockData + 1);
            blockData[blockSize + 1] = '\"';
            blockData[blockSize + 2] = 0;
            // std::cout << (char *)blockData << std::endl;

            bool submissionGood = SubmitBlock(blockData, blockSize + 3);
            // Logger::Log(LogType::Debug, LogField::Stratum, "Block hex: %.*s",
            //             blockSize + 3, blockData);
            delete[] blockData;
            block_submissions.push_back(
                new BlockSubmission(shareRes, cli->GetWorkerName(), time, job));

            SendAccept(cli, id);
            // total_effort_pow += shareRes.Diff;
        }
        break;
        case ShareCode::VALID_SHARE:
            SendAccept(cli, id);
            // total_effort_pow += shareRes.Diff;
            break;
        default:
            SendReject(cli, id, (int)shareRes.Code, shareRes.Message.c_str());
            break;
    }

    // write invalid shares too for statistics
    {
        std::lock_guard lock_db(db_mutex);
        bool dbRes =
            redis_manager.AddShare(cli->GetWorkerName(), time, shareRes.Diff);
    }
    // TODO: there may be bug if handle notify runs before add share, total
    // effort wont include block share HandleBlockNotify(*new
    // ondemand::array());

    auto duration = DIFF_US(end, start);

    Logger::Log(LogType::Debug, LogField::Stratum,
                "Share processed in %dus, diff: %f.", duration, shareRes.Diff);
}

void StratumServer::SendReject(StratumClient *cli, int id, int err,
                               const char *msg)
{
    char buffer[512];
    int len =
        snprintf(buffer, sizeof(buffer),
                 "{\"id\":%d,\"result\":null,\"error\":[%d,\"%s\",null]}\n", id,
                 err, msg);
    SendRaw(cli->GetSock(), buffer, len);
}

void StratumServer::SendAccept(StratumClient *cli, int id)
{
    char buff[512];
    int len = snprintf(buff, sizeof(buff),
                       "{\"id\":%d,\"result\":true,\"error\":null}\n", id);
    SendRaw(cli->GetSock(), buff, len);
}

bool StratumServer::SubmitBlock(const char *blockHex, int blockHexLen)
{
    std::vector<char> resultBody;
    int resCode =
        SendRpcReq(resultBody, 1, "submitblock", blockHex, blockHexLen);

    if (resCode != 200)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Failed to send block submission, http code: %d, res: %.*s",
                    resCode, resultBody.size(), resultBody.data());
        return false;
    }

    try
    {
        ondemand::document doc = httpParser.iterate(
            resultBody.data(), resultBody.size(), resultBody.capacity());

        ondemand::object res = doc.get_object();
        ondemand::value resultField = res["result"];
        ondemand::value errorField = res["error"];

        if (!errorField.is_null())
        {
            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Block submission rejected, rpc error: %s",
                        errorField.get_raw_json_string());
            return false;
        }

        if (!resultField.is_null())
        {
            std::string_view result = resultField.get_string();
            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Block submission rejected, rpc result: %.*s",
                        result.size(), result.data());

            if (result == "inconclusive")
            {
                Logger::Log(
                    LogType::Warn, LogField::Stratum,
                    "Submitted inconclusive block, waiting for result...");
                return true;
            }
            return false;
        }
    }
    catch (simdjson::simdjson_error &err)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Submit block response parse error: %s", err.what());
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
    // arith256.GetHex().c_str());

    SendRaw(cli->GetSock(), request, len);

    Logger::Log(LogType::Debug, LogField::Stratum, "Set difficulty to %s",
                arith256.GetHex().c_str());
}

void StratumServer::AdjustDifficulty(StratumClient *cli, int64_t curTime)
{
    auto period = curTime - cli->GetLastAdjusted();

    if (period < MIN_PERIOD_SECONDS) return;

    double minuteRate = ((double)60 / period) * (double)cli->GetShareCount();

    std::cout << "share minute rate: " << minuteRate << std::endl;

    // we allow 20% difference from target
    if (minuteRate > coin_config.target_shares_rate * 1.2 ||
        minuteRate < coin_config.target_shares_rate * 0.8)
    {
        std::cout << "old diff: " << cli->GetDifficulty() << std::endl;

        // similar to bitcoin difficulty calculation
        double newDiff = (minuteRate / coin_config.target_shares_rate) *
                         cli->GetDifficulty();
        if (newDiff == 0) newDiff = cli->GetDifficulty() / 5;

        cli->SetDifficulty(newDiff, curTime);
        this->UpdateDifficulty(cli);
        std::cout << "new diff: " << newDiff << std::endl;
    }
    cli->ResetShareCount();
}

void StratumServer::BroadcastJob(StratumClient *cli, Job *job)
{
    int res =
        SendRaw(cli->GetSock(), job->GetNotifyBuff(), job->GetNotifyBuffSize());
}

// TODO: add a lock job

int StratumServer::SendRaw(int sock, char *data, int len)
{
    // dont send sigpipe
    return send(sock, data, len, MSG_NOSIGNAL);
}

// TODO: make this like printf for readability
int StratumServer::SendRpcReq(std::vector<char> &result, int id,
                              const char *method, const char *params,
                              int paramsLen)
{
    std::lock_guard rpc_lock(rpc_mutex);
    for (DaemonRpc *rpc : rpcs)
    {
        int res = rpc->SendRequest(result, id, method, params, paramsLen);

        if (res != -1) return res;
    }

    return -2;
}
// TODO: make hashing multi threaded
// TODO: make jobs a set