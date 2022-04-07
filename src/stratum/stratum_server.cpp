#include "stratum_server.hpp"

CoinConfig StratumServer::coin_config;
std::vector<DaemonRpc *> StratumServer::rpcs;
JobManager StratumServer::job_manager;

StratumServer::StratumServer()
    : 
      reqParser(REQ_BUFF_SIZE),
      target_shares_rate(coin_config.target_shares_rate),
      redis_manager(RedisManager(coin_config.redis_host)),
      block_submission(nullptr)

{
    // coin_config = cnfg;
    // for (int i = 0; i < cnfg.rpcs.size(); i++)
    // {
    //     this->rpcs.push_back(new DaemonRpc(std::string(cnfg.rpcs[i].host),
    //                                        std::string(cnfg.rpcs[i].auth)));
    // }

    job_count = redis_manager.GetJobCount();

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
    addr.sin_port = htons(coin_config.stratum_port);

    if (bind(sockfd, (const sockaddr *)&addr, sizeof(addr)) != 0)
    {
        throw std::runtime_error("Failed to bind to port " +
                                 std::to_string(coin_config.stratum_port));
    }

    // init hash functions if needed
    HashWrapper::InitSHA256();
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

    std::thread listeningThread(&StratumServer::Listen, this);
    listeningThread.join();
}

void StratumServer::Listen()
{
    Logger::Log(LogType::Info, LogField::Stratum,
                "Started listenning on port: %d", ntohs(this->addr.sin_port));

    while (true)
    {
        int addr_len;
        int conn_fd;
        sockaddr_in conn_addr;
        conn_fd = accept(sockfd, (sockaddr *)&addr, (socklen_t *)&addr_len);

        std::cout << "tcp client connected, starting new thread..."
                  << std::endl;

        if (conn_fd <= 0)
        {
            std::cerr << "Invalid connecting socket accepted. Ignoring..."
                      << std::endl;
            continue;
        };

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
            buffer[total] = '\0';
            reqEnd = std::strchr(buffer, '\n');
        } while (reqEnd == NULL && total < REQ_BUFF_SIZE - 1);

        // exit loop, miner disconnected
        if (recvRes <= 0)
        {
            close(client->GetSock());
            clients_mutex.lock();
            clients.erase(std::find(clients.begin(), clients.end(), client));
            clients_mutex.unlock();
            Logger::Log(Info, LogField::Stratum,
                        "client disconnected. res: %d, errno: %d", recvRes,
                        errno);
            break;
        }
        // std::cout << "buff: " << buffer << std::endl;

        if (std::strchr(buffer, '\n') == NULL)
        {
            std::cout << "Request too big." << std::endl;
            reqStart = &buffer[0];
            while (strchr(reqStart, '{') != NULL)
            {
                reqStart = strchr(buffer, '{');
            }

            total = total - (&buffer[REQ_BUFF_SIZE] - reqStart);
            continue;
        }

        // there can be multiple messages in 1 recv
        while (reqEnd != NULL)
        {
            reqStart = strchr(buffer, '{');
            reqLen = reqEnd - reqStart;
            HandleReq(client, reqStart, reqLen);

            // nextReqLen = total - reqLen - 1;
            nextReqLen = std::strlen(reqEnd + 1);
            std::memmove(buffer, reqEnd + 1, nextReqLen);
            buffer[nextReqLen] = '\0';

            reqEnd = strchr(buffer, '\n');
        }

        total = nextReqLen;
        // std::cout << "total: " << total << std::endl;
    }
}

void StratumServer::HandleReq(StratumClient *cli, char buffer[], int reqSize)
{
    // std::cout << buffer << std::endl;
    int id = 0;
    std::string_view method;
    ondemand::array params;

    auto start = std::chrono::steady_clock::now();

    ondemand::document doc =
        reqParser.iterate(buffer, reqSize, REQ_BUFF_SIZE + SIMDJSON_PADDING);
    // std::cout << "last char -> " << (int)buffer[]
    try
    {
        ondemand::object req = doc.get_object();
        id = req["id"].get_int64();
        method = req["method"].get_string();
        params = req["params"].get_array();
    }
    catch (simdjson::simdjson_error &err)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "JSON parse error: %s\nRequest: %s\n", err.what(), buffer);
    }

    auto end = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    // std::cout << "req parse took: " << dur << "micro seconds." << std::endl;

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
    // TODO: verify the notification
    //  first of all clear jobs to insure we are not paying for stale shares
    jobs.clear();

    auto start = std::chrono::steady_clock::now();

    Job* job = nullptr;

    job = job_manager.GetNewJob();

    while (job == nullptr)
    {
        job = job_manager.GetNewJob();

        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Block update error: Failed to generate new job! retrying...");

        std::this_thread::sleep_for(250ms);
    }

//     try
//     {
//         // std::cout << resBody.data() << std::endl;

//         ondemand::document doc = httpParser.iterate(
//             resBody.data(), resBody.size() - SIMDJSON_PADDING,
//             resBody.size() + 1);

//         ondemand::object res = doc["result"].get_object();

//         // this must be in the order they appear in the result for simdjson
//         int version = res["version"].get_int64();
//         prevBlockHash = res["previousblockhash"].get_string();
// #if POOL_COIN == COIN_VRSCTEST
//         std::string_view finalSRoot = res["finalsaplingroothash"].get_string();
//         std::string_view solution = res["solution"].get_string();
//         // char solution[SOLUTION_SIZE] = {0};
//         // solution[1] = 7;
// #endif
//         ondemand::array txs = res["transactions"].get_array();
//         int value = res["coinbasetxn"]["coinbasevalue"].get_int64();
//         curtime = res["curtime"].get_int64(); //TODO: check order
//         mintime = res["mintime"].get_int64();
//         std::string_view bits = res["bits"].get_string();
//         newHeight = res["height"].get_int64();

//         std::vector<std::vector<unsigned char>> transactions(1);
//         transactions[0] = GetCoinbaseTx(value, mintime, newHeight);

//         auto end = std::chrono::steady_clock::now();

// #if POOL_COIN == COIN_VRSCTEST

//         // reverse all fields to header encoding
//         char prevBlockRev[64];
//         char finalSRootRev[64];
//         char bitsRev[8];
//         // char sol[SOLUTION_SIZE * 2];

//         ReverseHex(prevBlockRev, prevBlockHash.data(), 64);
//         ReverseHex(finalSRootRev, finalSRoot.data(), 64);
//         ReverseHex(bitsRev, bits.data(), 8);
//         // memcpy(sol144, solution.data(), SOLUTION_SIZE * 2);
//         // memcpy(sol144, solution, 144);

//         uint32_t versionRev = bswap_32(version);
//         uint32_t mintimeRev = bswap_32(mintime);

//         job = new VerusJob(this->job_count, transactions, true, version,
//                            prevBlockRev, mintime, bitsRev, finalSRootRev,
//                            solution.data());
// #endif

//         std::cout << "Generated job: " << job->GetId() << " in " << std::dec
//                   << std::chrono::duration_cast<std::chrono::microseconds>(
//                          end - start)
//                          .count()
//                   << "us" << std::endl;
//         // block->AddTransaction(coinbaseTx);
//         // for (ondemand::array::Constondemand::arrayIterator itr =
//         // txs.begin();
//         // itr
//         // !=
//         // txs.end();
//         // itr++)
//         //     block->AddTransaction((*itr)["data"].GetString());

//         this->redis_manager.SetJobCount(++this->job_count);

//         // std::cout << "Block update: " << height << std::endl;
//         std::cout << "Difficulty target: " << std::fixed << job->GetTargetDiff()
//                   << std::endl;
//         std::cout << "Broadcasted job: " << job->GetId() << std::endl;
//     }
//     catch (simdjson::simdjson_error &err)
//     {
//         std::cerr << "getblocktemplate json parse error: " << err.what()
//                   << std::endl;
//         return;
//     }

//     jobs.push_back(job);

//     for (auto it = clients.begin(); it != clients.end(); it++)
//     {
//         AdjustDifficulty(*it, mintime);
//         BroadcastJob(*it, job);

//         unsigned char test[32];
//         HashWrapper::VerushashV2b2(test, job->GetHeaderData(),
//                                    BLOCK_HEADER_SIZE, (*it)->GetHasher());
//     }

    // if (block_submission != nullptr)
    // {
    //     unsigned char prevBlockBin[32];
    //     Unhexlify(prevBlockBin, prevBlockHash.data(), prevBlockHash.size());

    //     bool blockAdded = true;

    //     // compare reversed
    //     for (int i = 0; i < 32; i++)
    //     {
    //         if (prevBlockBin[32 - i - 1] !=
    //             block_submission->shareRes.HashBytes[i])
    //         {
    //             blockAdded = false;
    //             break;
    //         }
    //     }

    //     if (blockAdded)
    //     {
    //         redis_manager.CloseRound(12, newHeight);
    //         Logger::Log(LogType::Info, LogField::Stratum,
    //                     "Block added to chain! found by: %s, hash: %.*s",
    //                     block_submission->worker.c_str(), prevBlockHash.size(),
    //                     prevBlockHash.data());
    //     }
    //     else
    //     {
    //         Logger::Log(LogType::Critical, LogField::Stratum,
    //                     "Block NOT added to chain %.*s!", prevBlockHash.size(),
    //                     prevBlockHash.data());
    //     }

    //     redis_manager.AddBlockSubmission(*block_submission, newHeight - 1,
    //                                      blockAdded, redis_manager.GetTotalEffort());

    //     delete block_submission;
    //     block_submission = nullptr;
    // }

    // std::vector<char> resBody;
    // // if (block_timestamps.size() < BLOCK_MATURITY)
    // if (mature_timestamp == 0)
    // {
    //     std::string params = "\""+ std::to_string(newHeight - 100 - 1) + "\"";
    //     int resCode =
    //         SendRpcReq(resBody, 1, "getblock", params.c_str(), params.size());

    //     ondemand::document doc = httpParser.iterate(
    //         resBody.data(), resBody.size() - SIMDJSON_PADDING,
    //         resBody.size());

    //     ondemand::object res = doc["result"].get_object();
    //     mature_timestamp = res["time"].get_uint64();
    //     redis_manager.SetMatureTimestamp(mature_timestamp);
    // }
    // else
    // {
    //     mature_timestamp = mature_timestamp + (curtime - last_block_timestamp);
    // }
    // last_block_timestamp = curtime;
    // Logger::Log(LogType::Info, LogField::Stratum, "Mature timestamp: %d",
    //             mature_timestamp);
}

void StratumServer::HandleWalletNotify(ondemand::array &params)
{
    std::string_view txId = (*params.begin()).get_string();

    std::cout << "Received transaction: " << txId << std::endl;

    // redis_manager.CloseRound(12, 1);
    // std::vector<char> resBody;
    // char rpcParams[64];
    // rpcParams[0] = '\"';
    // memcpy(rpcParams + 1, txId.data(), txId.size());
    // rpcParams[txId.size() + 1] = '\"';
    // rpcParams[txId.size() + 2] = ',';
    // rpcParams[txId.size() + 3] = '1';
    // int len = txId.size() + 4;

    // int resCode = SendRpcReq(resBody, 1, "getrawtransaction", rpcParams,
    // len);

    // if (resCode != 200)
    // {
    //     std::cerr << "CheckAcceptedBlock error: Failed to getrawtransaction,
    //     "
    //                  "http code: "
    //               << resCode << std::endl;
    //     // return CheckAcceptedBlock(height);
    // }

    // ondemand::document doc =
    //     httpParser.iterate(resBody.data(), resBody.size() - 32,
    //     resBody.size());

    // ProcessRound(1);
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

    std::cout << "client subscribed!" << std::endl;
}

void StratumServer::HandleAuthorize(StratumClient *cli, int id,
                                    ondemand::array &params)
{
    int split = 0, resCode = 0;
    std::string reqParams;
    std::string_view given_addr, valid_addr, worker, identity = "null";
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
            doc = httpParser.iterate(resultBody.data(),
                                     resultBody.size() - SIMDJSON_PADDING,
                                     resultBody.size());

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
                    doc = httpParser.iterate(
                        resultBody.data(), resultBody.size() - SIMDJSON_PADDING,
                        resultBody.size());

                    res = doc["result"].get_object();
                    identity = res["name"];
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
                identity = given_addr;
            }
        }
        redis_manager.AddAddress(valid_addr, identity);
    }

    std::string worker_full_str =
        std::string(std::string(valid_addr) + "." + std::string(worker));
    worker_full = worker_full_str;

    cli->SetWorkerFull(worker_full);
    redis_manager.AddWorker(valid_addr, worker_full);

    Logger::Log(LogType::Info, LogField::Stratum,
                "Authorized worker: %.*s, address: %.*s, id: %.*s",
                worker.size(), worker.data(), valid_addr.size(),
                valid_addr.data(), identity.size(), identity.data());

    SendAccept(cli, id);
    this->UpdateDifficulty(cli);

    if (jobs.size() == 0)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "no job to broadcast to connected client!");
        return;
    }

    this->BroadcastJob(cli, jobs.back());
}

// https://zips.z.cash/zip-0301#mining-submit
void StratumServer::HandleSubmit(StratumClient *cli, int id,
                                 ondemand::array &params)
{
    // parsing takes 0-1 us
    auto start = steady_clock::now();
    Share share;
    auto it = params.begin();
    share.worker = (*it).get_string();
    share.jobId = (*++it).get_string();
    share.time = (*++it).get_string();
    share.nonce2 = (*++it).get_string();
    share.solution = (*++it).get_string();

    auto end = steady_clock::now();
    HandleShare(cli, id, share);
    // auto duration = duration_cast<microseconds>(end - start).count();
}

void StratumServer::HandleShare(StratumClient *cli, int id, const Share &share)
{
    struct timeval time_now;
    gettimeofday(&time_now, nullptr);
    std::time_t time = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    ShareResult shareRes;

    auto start = TIME_NOW();
    Job *job = GetJobById(share.jobId);

    if (job == nullptr)
    {
        shareRes.Code = ShareCode::JOB_NOT_FOUND;
        shareRes.Message = "Job not found";
        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Received share for unknown job id: %s", share.jobId);
    }
    else
    {
        shareRes = ShareProcessor::Process(time, *cli, *job, share);
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
            job->GetBlockHex(blockData + 1);
            blockData[blockSize + 1] = '\"';
            blockData[blockSize + 2] = 0;
            // std::cout << (char *)blockData << std::endl;

            bool submissionGood = SubmitBlock(blockData, blockSize + 3);
            Logger::Log(LogType::Debug, LogField::Stratum, "Block hex: %s",
                        blockData);
            delete[] blockData;
            block_submission =
                new BlockSubmission(shareRes, cli->GetWorkerName(), time, job);
            SendAccept(cli, id);
        }
        break;
        case ShareCode::VALID_SHARE:
            SendAccept(cli, id);
            break;
        default:
            SendReject(cli, id, (int)shareRes.Code, shareRes.Message);
            break;
    }

    // write invalid shares too for statistics
    bool dbRes =
        redis_manager.AddShare(cli->GetWorkerName(), time, shareRes.Diff);

    auto duration = DIFF_US(end, start);
    Logger::Log(LogType::Debug, LogField::Stratum,
                "Share processed in %dus, diff: %f.", duration, shareRes.Diff);
    return;

    // cli->SetLastShare(time);

    // if (job == nullptr)
    // {
    //     redis_manager.AddShare(share.worker, STALE_SHARE_DIFF, time);
    //     return RejectShare(cli, id, ShareCode::JOB_NOT_FOUND);
    // }

    // TODO: check duplicate

    // unsigned char *headerData = job->GetHeaderData(
    //     share.time, cli->GetExtraNonce(), share.nonce2, share.solution);

    // unsigned char hashBuff[32];

    // std::cout << "header: ";
    // for (int i = 0; i < BLOCK_HEADER_SIZE; i++)
    //     std::cout << std::hex << std::setfill('0') << std::setw(2)
    //               << (int)headerData[i];
    // std::cout << std::endl;
    // auto start = std::chrono::steady_clock::now();
    // HashWrapper::VerushashV2b2(hashBuff, headerData, BLOCK_HEADER_SIZE,
    //                            cli->GetHasher());

    // auto end = std::chrono::steady_clock::now();
    // auto duration =
    //     std::chrono::duration_cast<microseconds>(end - start).count();
    // std::cout << "hash dur: " << duration << "microseconds." << std::endl;
    /* code */

    // std::vector<unsigned char> v(hashBuff, hashBuff + 32);
    // uint256 hash256(v);

    // double shareDiff = BitsToDiff(UintToArith256(hash256).GetCompact(false));

    // Logger::Log(Debug, Stratum, "block hash      :%s ", hash256.GetHex());
    // std::cout << "block difficulty: " << job->GetTargetDiff() << std::endl;
    // std::cout << "share difficulty: " << shareDiff << std::endl;
    // std::cout << "client target   : " << cli->GetDifficulty() << std::endl;
    // if (shareDiff >= job->GetTargetDiff())
    // {
    //     std::cout << "Found block solution!" << std::endl;
    //     int blockSize = job->GetBlockSize();
    //     char *blockData = new char[blockSize + 3];
    //     blockData[0] = '\"';
    //     job->GetBlockHex(blockData + 1);
    //     blockData[blockSize + 1] = '\"';
    //     blockData[blockSize + 2] = 0;
    //     // std::cout << (char *)blockData << std::endl;

    //     bool submissionGood = SubmitBlock(blockData, blockSize + 3);
    //     delete[] blockData;
    //     if (submissionGood)
    //     {
    //         std::cout << "Submit block successful." << std::endl;
    //         // redis_manager->InsertPendingBlock(hash_str);
    //     }
    //     else
    //     {
    //         std::cerr << "Error submitting block, retrying..." << std::endl;
    //         // submissionGood = SubmitBlock(blockHex);
    //     }
    // }
    // else if (shareDiff < cli->GetDifficulty())
    // {
    //     redis_manager.AddShare(share.worker, INVALID_SHARE_DIFF, time);
    //     return RejectShare(cli, id, ShareResult::LOW_DIFFICULTY_SHARE);
    // }

    // char buffer[1024];
    // int len = snprintf(resBuff, sizeof(resBuff),
    //                    "{\"id\":%d,\"result\":true,\"error\":null}\n", id);
    // send(cli->GetSock(), buffer, len, 0);
    // redis_manager.AddShare(share.worker, shareDiff, time);
    // std::cout << "share accepted" << std::endl;
}

void StratumServer::SendReject(StratumClient *cli, int id, int err,
                               const char *msg)
{
    char buffer[512];
    int len =
        snprintf(buffer, sizeof(buffer),
                 "{\"id\":%d,\"result\":null,\"error\":[%d,\"%s\",null]}\n", id,
                 err, msg);
    send(cli->GetSock(), buffer, len, 0);
}

void StratumServer::SendAccept(StratumClient *cli, int id)
{
    char buff[512];
    int len = snprintf(buff, sizeof(buff),
                       "{\"id\":%d,\"result\":true,\"error\":null}\n", id);
    send(cli->GetSock(), buff, len, 0);
}

bool StratumServer::SubmitBlock(const char *blockHex, int blockHexLen)
{
    std::vector<char> resultBody;
    int resCode =
        SendRpcReq(resultBody, 1, "submitblock", blockHex, blockHexLen);

    if (resCode != 200)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Failed to send block submission, http code: %d", resCode);
        return false;
    }

    try
    {
        ondemand::document doc = httpParser.iterate(
            resultBody.data(), resultBody.size() - SIMDJSON_PADDING,
            resultBody.size());

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
                    "Submitting inconclusive block, waiting for result...");
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

    send(cli->GetSock(), request, len, 0);

    Logger::Log(LogType::Debug, LogField::Stratum, "Set difficulty to %s",
                arith256.GetHex().c_str());
}

void StratumServer::AdjustDifficulty(StratumClient *cli, std::time_t curTime)
{
    std::time_t period = curTime - cli->GetLastAdjusted();

    if (period < MIN_PERIOD_SECONDS) return;

    double minuteRate = ((double)60 / period) * (double)cli->GetShareCount();

    std::cout << "share minute rate: " << minuteRate << std::endl;

    // we allow 20% difference from target
    if (minuteRate > target_shares_rate * 1.2 ||
        minuteRate < target_shares_rate * 0.8)
    {
        std::cout << "old diff: " << cli->GetDifficulty() << std::endl;

        // similar to bitcoin difficulty calculation
        double newDiff =
            (minuteRate / target_shares_rate) * cli->GetDifficulty();
        if (newDiff == 0) newDiff = cli->GetDifficulty() / 5;

        cli->SetDifficulty(newDiff, curTime);
        this->UpdateDifficulty(cli);
        std::cout << "new diff: " << newDiff << std::endl;
    }
    cli->ResetShareCount();
}

void StratumServer::BroadcastJob(StratumClient *cli, Job *job)
{
    send(cli->GetSock(), job->GetNotifyBuff(), job->GetNotifyBuffSize(), 0);
}

std::vector<unsigned char> StratumServer::GetCoinbaseTx(int64_t value,
                                                        uint32_t curtime,
                                                        uint32_t height)
{
    // maximum scriptsize = 100
    unsigned char prevTxIn[32] = {0};  // null last input
    const char extraData[] = "SickPool is in the building.";
    const int extraDataLen = sizeof(extraData) - 1;

    std::vector<unsigned char> heightScript = GetNumScript(height);
    const unsigned char heightScriptLen = heightScript.size();

    std::vector<unsigned char> signature(heightScript.size() + extraDataLen);

    memcpy(signature.data(), &heightScriptLen, 1);
    memcpy(signature.data() + 1, heightScript.data(), heightScript.size());
    memcpy(signature.data() + 1 + heightScript.size(), &extraData,
           extraDataLen);

    // todo: make txVersionGroup updateable without reset
    uint32_t txVersionGroup = 0x892f2085;
    VerusTransaction coinbaseTx(4, curtime, true, txVersionGroup);
    coinbaseTx.AddInput(prevTxIn, UINT32_MAX, signature, UINT32_MAX);
    coinbaseTx.AddP2PKHOutput(this->coin_config.pool_addr,
                            value);  // TODO: add i address check
    coinbaseTx.AddTestnetCoinbaseOutput();
    return coinbaseTx.GetBytes();
}

// TODO: add a lock jobs
Job *StratumServer::GetJobById(std::string_view id)
{
    for (auto it = jobs.begin(), end = jobs.end(); it != end; it++)
    {
        if (std::strncmp((*it)->GetId(), id.data(), id.size()) == 0)
        {
            return (*it);
        }
    }
    return nullptr;
}


//TODO: make this like printf for readability
int StratumServer::SendRpcReq(std::vector<char> &result, int id,
                              const char *method, const char *params,
                              int paramsLen)
{
    for (DaemonRpc *rpc : rpcs)
    {
        int res = rpc->SendRequest(result, id, method, params, paramsLen);
        if (res != -1) return res;
    }

    return -2;
}