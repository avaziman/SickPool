#include "stratum_server.hpp"

StratumServer::StratumServer(const CoinConfig &conf)
    : coin_config(conf),
      reqParser(REQ_BUFF_SIZE),
      httpParser(MAX_HTTP_REQ_SIZE),
      redis_manager(),
      stats_manager(redis_manager.rc, &redis_mutex,
                    (int)coin_config.hashrate_interval_seconds,
                    (int)coin_config.effort_interval_seconds,
                    (int)coin_config.average_hashrate_interval_seconds,
                    (int)coin_config.hashrate_ttl_seconds),
      daemon_manager(coin_config.rpcs),
      job_manager(&daemon_manager, coin_config.pool_addr),
      submission_manager(&redis_manager, &daemon_manager, &stats_manager)
{
    // never grow beyond this size
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
        throw std::runtime_error("Stratum server failed to bind to port " +
                                 std::to_string(coin_config.stratum_port));
    }

    // init hash functions if needed
    HashWrapper::InitSHA256();
#if POOL_COIN <= COIN_VRSC
    HashWrapper::InitVerusHash();
#endif

    redis_manager.ResetWorkerCount();
    // redis_manager.UpdatePoS(0, GetCurrentTimeMs());

    std::thread stats_thread(&StatsManager::Start, &stats_manager);
    stats_thread.detach();

    control_server.Start(1111);
    std::thread control_thread(&StratumServer::HandleControlCommands, this);
    control_thread.detach();
}

StratumServer::~StratumServer()
{
    // for (std::unique_ptr<StratumClient> cli : this->clients){
    //     close(cli.get()->GetSock());
    // }
    close(this->sockfd);
}

void StratumServer::HandleControlCommands()
{
    while (true)
    {
        ControlCommands cmd = control_server.GetNextCommand();
        HandleControlCommand(cmd);
    }
}

void StratumServer::HandleControlCommand(ControlCommands cmd)
{
    switch (cmd)
    {
        case ControlCommands::UPDATE_BLOCK:
            auto val = simdjson::ondemand::array();
            HandleBlockNotify(val);
            break;
    }
}

void StratumServer::StartListening()
{
    if (listen(this->sockfd, 1024) != 0)
        throw std::runtime_error(
            "Stratum server failed to enter listenning state.");

    auto val = simdjson::ondemand::array();
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
    auto client = std::make_unique<StratumClient>(conn_fd, time(nullptr),
                                                  coin_config.default_diff);
    auto cli_ptr = client.get();

    {
        std::scoped_lock cli_lock(clients_mutex);
        // client will be null after move, so assign the new ref to it
        this->clients.push_back(std::move(client));
        // client is moved, do not use
    }

    // simdjson requires some extra bytes, so don't write to the last bytes
    char buffer[REQ_BUFF_SIZE];
    char *reqEnd = nullptr, *reqStart = nullptr;
    int total = 0, reqLen = 0, nextReqLen = 0;
    std::size_t recvRes = 0;
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
        } while (reqEnd == nullptr && total < REQ_BUFF_SIZE_REAL);

        // exit loop, miner disconnected
        if (recvRes <= 0)
        {
            stats_manager.PopWorker(cli_ptr->GetFullWorkerName(),
                                    cli_ptr->GetAddress());
            close(cli_ptr->GetSock());

            {
                std::scoped_lock cli_lock(clients_mutex);
                // TODO: clean up clients
                //  clients.erase(
                //      std::find(clients.begin(), clients.end(), cli_ptr));
            }

            Logger::Log(LogType::Info, LogField::Stratum,
                        "client disconnected. res: %d, errno: %d", recvRes,
                        errno);
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
            HandleReq(cli_ptr, reqStart, reqLen);

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
    simdjson::ondemand::array params;

    auto start = std::chrono::steady_clock::now();

    // std::cout << "last char -> " << (int)buffer[]
    simdjson::ondemand::document doc;
    try
    {
        doc = reqParser.iterate(buffer, reqSize, REQ_BUFF_SIZE);

        simdjson::ondemand::object req = doc.get_object();
        id = static_cast<int>(req["id"].get_int64());
        method = req["method"].get_string();
        params = req["params"].get_array();
    }
    catch (const simdjson::simdjson_error &err)
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

    if (method == "mining.submit")
    {
        HandleSubmit(cli, id, params);
    }
    else if (method == "mining.subscribe")
    {
        HandleSubscribe(cli, id, params);
    }
    else if (method == "mining.authorize")
    {
        HandleAuthorize(cli, id, params);
    }
    else
    {
        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Unknown request method: %.*s", method.size(),
                    method.data());
    }
}

void StratumServer::HandleBlockNotify(const simdjson::ondemand::array &params)
{
    using namespace simdjson;
    // TODO: verify the notification
    int64_t curtimeMs = GetCurrentTimeMs();

    const job_t *newJob = job_manager.GetNewJob();

    while (newJob == nullptr)
    {
        newJob = job_manager.GetNewJob();

        Logger::Log(
            LogType::Critical, LogField::Stratum,
            "Block update error: Failed to generate new job! retrying...");

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    // save it to process round
    {
        std::scoped_lock clients_lock(clients_mutex);
        for (auto it = clients.begin(); it != clients.end(); it++)
        {
            // (*it)->SetDifficulty(1000000, curtimeMs);
            // (*it)->SetDifficulty(job->GetTargetDiff(), curtimeMs);
            // UpdateDifficulty(*it);
            BroadcastJob((*it).get(), newJob);
        }
    }

    // for (auto it = clients.begin(); it != clients.end(); it++)
    // {
    //     AdjustDifficulty(*it, curtime);
    // }

    // in case of a crash without shares, we need to update round start time
    // redis_manager.SetNewRoundTime(round_start_pow);

    std::scoped_lock redis_lock(redis_mutex);

    // while (jobs.size() > 1)
    // {
    //     // TODO: fix
    //     //  delete jobs[0];
    //     //  jobs.erase();
    // }

    redis_manager.AddNetworkHr(curtimeMs, newJob->GetTargetDiff());

    redis_manager.SetEstimatedNeededEffort(newJob->GetEstimatedShares());
    // TODO: combine all redis functions one new block to one pipelined
    // Logger::Log(LogType::Info, LogField::Stratum, "Mature timestamp: %"
    // PRId64,
    //             mature_timestamp_ms);

    Logger::Log(LogType::Info, LogField::JobManager,
                "Broadcasted new job: #%.*s", newJob->GetId().length(),
                newJob->GetId().data());
    Logger::Log(LogType::Info, LogField::JobManager, "Height: %d",
                newJob->GetHeight());
    Logger::Log(LogType::Info, LogField::JobManager, "Min time: %d",
                newJob->GetMinTime());
    Logger::Log(LogType::Info, LogField::JobManager, "Difficutly: %f",
                newJob->GetTargetDiff());
    Logger::Log(LogType::Info, LogField::JobManager, "Est. shares: %f",
                newJob->GetEstimatedShares());
    // Logger::Log(LogType::Info, LogField::JobManager, "Target: %s",
    //             job->GetTarget()->GetHex().c_str());
    Logger::Log(LogType::Info, LogField::JobManager, "Block reward: %d",
                newJob->GetBlockReward());
    Logger::Log(LogType::Info, LogField::JobManager, "Transaction count: %d",
                newJob->GetTransactionCount());
    Logger::Log(LogType::Info, LogField::JobManager, "Block size: %d",
                newJob->GetBlockSize());
}

void StratumServer::HandleWalletNotify(simdjson::ondemand::array &params)
{
    using namespace simdjson;
    std::string_view txId = (*params.begin()).get_string();

    Logger::Log(LogType::Info, LogField::Stratum, "Received TxId: %.*s",
                txId.size(), txId.data());
    // redis_manager.CloseRound(12, 1);
    return;
    std::string resBody;
    char rpcParams[64];

    int resCode = daemon_manager.SendRpcReq<std::any>(
        resBody, 1, "getrawtransaction", std::any(txId),
        std::any(1));  // verboose

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
    catch (const simdjson_error &err)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "HandleWalletNotify: Failed to parse json, error: %s",
                    err.what());
        return;
    }

    resCode = daemon_manager.SendRpcReq<std::any>(resBody, 1, "getblock",
                                                  std::any(blockHash));

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
                                    simdjson::ondemand::array &params)
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
    int len =
        snprintf(response, sizeof(response),
                 "{\"id\":%d,\"result\":[null,\"%.*s\"],\"error\":null}\n", id,
                 8, cli->GetExtraNonce().data());
    // std::cout << response << std::endl;
    SendRaw(cli->GetSock(), response, len);

    std::cout << "client subscribed!" << std::endl;
}

void StratumServer::HandleAuthorize(StratumClient *cli, int id,
                                    simdjson::ondemand::array &params)
{
    using namespace simdjson;

    int split = 0, resCode = 0;
    std::string reqParams;
    std::string_view given_addr, valid_addr, worker, idTag = "null";
    bool isValid = false, isIdentity = false;

    std::string resultBody;
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
    else if (worker_full.size() > MAX_WORKER_NAME_LEN + ADDRESS_LEN)
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

        resCode = daemon_manager.SendRpcReq<std::any>(
            resultBody, 1, "validateaddress", std::any(given_addr));
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
        catch (const simdjson_error &err)
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
                resCode = daemon_manager.SendRpcReq<std::any>(
                    resultBody, 1, "getidentity", std::any(valid_addr));

                try
                {
                    doc =
                        httpParser.iterate(resultBody.data(), resultBody.size(),
                                           resultBody.capacity());

                    res = doc["result"].get_object();
                    idTag = res["name"];
                }
                catch (const simdjson_error &err)
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
    }

    std::string worker_full_str =
        std::string(std::string(valid_addr) + "." + std::string(worker));
    worker_full = worker_full_str;

    cli->HandleAuthorized(worker_full, valid_addr);
    // string-views to non-local string
    stats_manager.AddWorker(cli->GetAddress(), cli->GetFullWorkerName(),
                            std::time(nullptr));

    Logger::Log(LogType::Info, LogField::Stratum,
                "Authorized worker: %.*s, address: %.*s, id: %.*s",
                worker.size(), worker.data(), valid_addr.size(),
                valid_addr.data(), idTag.size(), idTag.data());

    SendAccept(cli, id);
    this->UpdateDifficulty(cli);

    // TODO: check null
    this->BroadcastJob(cli, job_manager.GetLastJob());
}

// https://zips.z.cash/zip-0301#mining-submit
void StratumServer::HandleSubmit(StratumClient *cli, int id,
                                 simdjson::ondemand::array &params)
{
    // parsing takes 0-1 us
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
    catch (const simdjson::simdjson_error &err)
    {
        SendReject(cli, id, (int)ShareCode::UNKNOWN, "invalid params");
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Failed to parse submit: %s", err.what());
        return;
    }

    HandleShare(cli, id, share);
}

void StratumServer::HandleShare(StratumClient *cli, int id, Share &share)
{
    int64_t time = GetCurrentTimeMs();
    auto start = TIME_NOW();

    ShareResult shareRes;

    // we don't write to job just read so no lock needed
    const job_t *job = job_manager.GetJob(share.jobId);

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
        ShareProcessor::Process(time, *cli, *job, share, shareRes);
        // shareRes.Code = ShareCode::VALID_BLOCK;
    }
    auto end = TIME_NOW();

    switch (shareRes.Code)
    {
        case ShareCode::VALID_BLOCK:
        {
            std::size_t blockSize = job->GetBlockSize();
            char blockData[blockSize];

            job->GetBlockHex(cli->GetBlockheaderBuff(), blockData);

            auto chain = std::string_view{"VRSCTEST"};
            // submit ASAP
            auto block_hex = std::string_view(blockData, blockSize);
            submission_manager.TrySubmit(chain, block_hex);

            const auto chainRound = stats_manager.GetChainRound(chain);
            auto submission_ptr =
                std::make_unique<BlockSubmission>(chain, cli->GetFullWorkerName(), job, shareRes,
                                 chainRound, time, block_number);
            submission_manager.AddImmatureBlock(std::move(submission_ptr));
            SendAccept(cli, id);
        }
        break;
        case ShareCode::VALID_SHARE:
            SendAccept(cli, id);
            break;
        default:
            SendReject(cli, id, (int)shareRes.Code, shareRes.Message.c_str());
            break;
    }
    // auto end = TIME_NOW();

    // write invalid shares too for statistics
    // possible that a timeseries wasn't created yet, so don't add shares
    if (shareRes.Code != ShareCode::UNAUTHORIZED_WORKER)
    {
        stats_manager.AddShare(cli->GetFullWorkerName(), cli->GetAddress(),
                               shareRes.Diff);
    }
    // bool dbRes =
    //     redis_manager.AddShare(cli->GetWorkerName(), time, shareRes.Diff);
    // TODO: there may be bug if handle notify runs before add share, total
    // effort wont include block share HandleBlockNotify(*new
    // ondemand::array());

    auto duration = DIFF_US(end, start);

    // Logger::Log(LogType::Debug, LogField::Stratum,
    //             "Share processed in %dus, diff: %f, res: %d", duration,
    //             shareRes.Diff, (int)shareRes.Code);
}

void StratumServer::SendReject(const StratumClient *cli, int id, int err,
                               const char *msg)
{
    char buffer[512];
    int len =
        snprintf(buffer, sizeof(buffer),
                 "{\"id\":%d,\"result\":null,\"error\":[%d,\"%s\",null]}\n", id,
                 err, msg);
    SendRaw(cli->GetSock(), buffer, len);
}

void StratumServer::SendAccept(const StratumClient *cli, int id)
{
    char buff[512];
    int len = snprintf(buff, sizeof(buff),
                       "{\"id\":%d,\"result\":true,\"error\":null}\n", id);
    SendRaw(cli->GetSock(), buff, len);
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

void StratumServer::BroadcastJob(const StratumClient *cli, const Job *job)
{
    auto res =
        SendRaw(cli->GetSock(), job->GetNotifyBuff(), job->GetNotifyBuffSize());
}

std::size_t StratumServer::SendRaw(int sock, const char *data,
                                   std::size_t len) const
{
    // dont send sigpipe
    return send(sock, data, len, MSG_NOSIGNAL);
}
