#include "stratum_server.hpp"

StratumServer::StratumServer(const CoinConfig &conf)
    : coin_config(conf),
      redis_manager("127.0.0.1", (int)conf.redis_port),
      diff_manager(&clients, &clients_mutex, coin_config.target_shares_rate),
      round_manager_pow(&redis_manager, "pow"),
      round_manager_pos(&redis_manager, "pos"),
      stats_manager(&redis_manager, &diff_manager, &round_manager_pow,
                    (int)coin_config.hashrate_interval_seconds,
                    (int)coin_config.effort_interval_seconds,
                    (int)coin_config.average_hashrate_interval_seconds,
                    (int)coin_config.diff_adjust_seconds,
                    (int)coin_config.hashrate_ttl_seconds),
      daemon_manager(coin_config.rpcs),
      job_manager(&daemon_manager, coin_config.pool_addr),
      submission_manager(&redis_manager, &daemon_manager, &round_manager_pow,
                         &round_manager_pos)
{
    auto error = httpParser.allocate(MAX_HTTP_REQ_SIZE, MAX_HTTP_JSON_DEPTH);
    if (error != simdjson::SUCCESS)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Failed to allocate http parser buffer: {}",
                    simdjson::error_message(error));
        exit(EXIT_FAILURE);
    }

    // init hash functions if needed
    HashWrapper::InitSHA256();
#if POW_ALGO == POW_ALGO_VERUSHASH
    HashWrapper::InitVerusHash();
#endif

    // redis_manager.UpdatePoS(0, GetCurrentTimeMs());

    std::thread stats_thread(&StatsManager::Start, &stats_manager);
    stats_thread.detach();

    control_server.Start(coin_config.control_port);
    std::thread control_thread(&StratumServer::HandleControlCommands, this);
    control_thread.detach();
}

StratumServer::~StratumServer()
{
    // for (std::unique_ptr<StratumClient> cli : this->clients){
    //     close(cli.get()->GetSock());
    // }
    close(listening_fd);
}

void StratumServer::HandleControlCommands()
{
    char buff[256] = {0};
    while (true)
    {
        ControlCommands cmd = control_server.GetNextCommand(buff, sizeof(buff));
        HandleControlCommand(cmd, buff);
        Logger::Log(LogType::Info, LogField::ControlServer,
                    "Processed control command: {}", buff);
    }
}

void StratumServer::HandleControlCommand(ControlCommands cmd, char buff[])
{
    switch (cmd)
    {
        case ControlCommands::BLOCK_NOTIFY:
            HandleBlockNotify();
            break;
        case ControlCommands::WALLET_NOTFIY:
            // format: %b%s%w (block hash, txid, wallet address)
            WalletNotify *wallet_notify =
                reinterpret_cast<WalletNotify *>(buff + 2);
            HandleWalletNotify(wallet_notify);
            break;
    }
}

void StratumServer::StartListening()
{
    HandleBlockNotify();

    std::thread listeningThread(&StratumServer::Listen, this);
    listeningThread.join();
}

void StratumServer::Listen()
{
    Logger::Log(LogType::Info, LogField::Stratum,
                "Started listenning on port: {}", coin_config.stratum_port);

    int epoll_fd = epoll_create1(0);

    if (epoll_fd == -1)
    {
        throw std::runtime_error(fmt::format(
            "Failed to create epoll: {} -> {}.", errno, std::strerror(errno)));
    }

    // throws if goes wrong
    int listener_fd = CreateListeningSock(epoll_fd);

    if (listen(listener_fd, MAX_CONNECTIONS) == -1)
        throw std::runtime_error(
            "Stratum server failed to enter listenning state.");

    // char ip_str[INET_ADDRSTRLEN];
    // struct in_addr ip_addr = conn_addr.sin_addr;
    // inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));

    // Logger::Log(LogType::Info, LogField::Stratum,
    //             "Tcp client connected, ip: {}, starting new thread...",
    //             ip_str);
    std::vector<std::thread> threads;

    const auto worker_amount = std::thread::hardware_concurrency();
    for (auto i = 0; i < worker_amount; i++)
    {
        threads.emplace_back(&StratumServer::ServiceSockets, this, epoll_fd,
                             listener_fd);
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // std::thread cliHandler(&StratumServer::HandleSocket, this, conn_fd);
    // // SetHighPriorityThrea d(cliHandler);
    // cliHandler.detach();
}

void StratumServer::ServiceSockets(int epoll_fd, int listener_fd)
{
    epoll_event events[MAX_CONNECTION_EVENTS];

    WorkerContext wc;
    struct sockaddr_in conn_addr;
    socklen_t addr_len = sizeof(conn_addr);
    int conn_fd;
    int epoll_res;
    int serviced_fd;
    ssize_t recv_res;

    Logger::Log(LogType::Info, LogField::Stratum,
                "Starting servicing sockets on thread {}", gettid());

    while (true)
    {
        epoll_res =
            epoll_wait(epoll_fd, events, MAX_CONNECTION_EVENTS, EPOLL_TIMEOUT);

        if (epoll_res == -1)
        {
            // TODO: think what to do here
            if (errno == EBADF || errno == EFAULT || errno == EINVAL)
            {
                throw std::runtime_error(
                    fmt::format("Failed to epoll_wait: {} -> {}", errno,
                                std::strerror(errno)));
            }
            else
            {
                // EINTR
                Logger::Log(LogType::Error, LogField::Stratum,
                            "Failed to epoll_wait: {} -> {}", errno,
                            std::strerror(errno));
                continue;
            }
        }

        for (int i = 0; i < epoll_res; i++)
        {
            serviced_fd = events[i].data.fd;

            if (serviced_fd == listener_fd)
            {
                // new connection
                conn_fd = AcceptConnection(epoll_fd, listener_fd, &conn_addr,
                                           &addr_len);

                if (conn_fd < 0)
                {
                    Logger::Log(LogType::Warn, LogField::Stratum,
                                "Failed to accept connecting socket errno: {} "
                                "-> errno: {}. ",
                                conn_fd, errno);
                    close(conn_fd);
                    continue;
                }

                AddClient(conn_fd);
            }
            else
            {
                // new data ready to be read
                HandleReadySocket(serviced_fd, &wc);
            }
        }
    }
}

void StratumServer::HandleReadySocket(int sockfd, WorkerContext *wc)
{
    StratumClient *cli = GetClient(sockfd);
    std::size_t req_len;
    std::size_t next_req_len;
    ssize_t recv_res;
    char *last_req_end;
    char *req_end;
    char *req_start;
    char *buffer = cli->req_buff;

    recv_res = recv(sockfd, cli->req_buff + cli->req_pos,
                    REQ_BUFF_SIZE_REAL - cli->req_pos - 1, 0);

    if (recv_res == -1)
    {
        if (errno == EWOULDBLOCK /*|| errno == EAGAIN*/)
        {
        }
        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Client disconnected because of socket error: {} -> {}.",
                    errno, std::strerror(errno));
        close(sockfd);

        return;
    }
    else if (recv_res == 0)
    {
        // TODO: print ip
        Logger::Log(LogType::Info, LogField::Stratum, "Client disconnected.");
        close(sockfd);
        return;
    }

    cli->req_pos += recv_res;
    buffer[cli->req_pos] = '\0';  // for strchr
    req_end = std::strchr(buffer, '\n');

    // there can be multiple messages in 1 recv
    // {1}\n{2}\n
    req_start = strchr(buffer, '{');  // "should" be first char
    while (req_start && req_end)
    {
        req_len = req_end - req_start;
        HandleReq(cli, wc, std::string_view(req_start, req_len));

        last_req_end = req_end;
        req_start = strchr(req_end + 1, '{');
        req_end = strchr(req_end + 1, '\n');
    }

    // if we haven't received a full request then don't touch the buffer
    if (req_end && req_start)
    {
        next_req_len =
            cli->req_pos - (last_req_end - buffer + 1);  // don't inlucde \n
        std::memmove(buffer, last_req_end + 1, next_req_len);
        buffer[next_req_len] = '\0';

        cli->req_pos = next_req_len;
    }
}
// TODO: test buffer too little

int StratumServer::CreateListeningSock(int epfd)
{
    int optval = 1;

    int listener_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener_fd == -1)
        throw std::runtime_error("Failed to create stratum socket");

    if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) == -1)
        throw std::runtime_error("Failed to set stratum socket options");

    // struct timeval timeout;
    // timeout.tv_sec = coin_config.socket_recv_timeout_seconds;
    // timeout.tv_usec = 0;
    // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
    // sizeof(timeout)) == -1)
    //     throw std::runtime_error("Failed to set stratum socket options");
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(coin_config.stratum_port));

    if (bind(listener_fd, (const sockaddr *)&addr, sizeof(addr)) == -1)
    {
        throw std::runtime_error(
            fmt::format("Stratum server failed to bind to port: {}",
                        coin_config.stratum_port));
    }

    epoll_event listener_ev;
    listener_ev.events = EPOLLIN | EPOLLET;
    listener_ev.data.fd = listener_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listener_fd, &listener_ev) == -1)
    {
        throw std::runtime_error(
            fmt::format("Failed to add listener socket to epoll set: {} -> {}",
                        errno, std::strerror(errno)));
    }

    return listener_fd;
}

int StratumServer::AcceptConnection(int epfd, int listener_fd,
                                    sockaddr_in *addr, socklen_t *addr_size)
{
    epoll_event conn_ev;
    int flags = O_NONBLOCK;
    int conn_fd = accept4(listener_fd, (sockaddr *)addr, addr_size, flags);

    if (conn_fd == -1)
    {
        return -1;
    }

    conn_ev.data.fd = conn_fd;
    conn_ev.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &conn_ev) == -1)
    {
        return -2;
    }

    return conn_fd;
}

void StratumServer::HandleSocket(int conn_fd)
{
    // auto client = std::make_unique<StratumClient>(conn_fd,
    // GetCurrentTimeMs(),
    //                                               coin_config.default_diff);
    // auto cli_ptr = client.get();

    // {
    //     std::scoped_lock cli_lock(clients_mutex);
    //     // client will be null after move, so assign the new ref to it
    //     this->clients.(std::move(client));
    //     // client is moved, do not use
    // }

    // simdjson requires some extra bytes, so don't write to the last bytes
    // char buffer[REQ_BUFF_SIZE];
    // char *reqEnd = nullptr;
    // char *reqStart = nullptr;
    // const char *lastReqEnd = nullptr;
    // std::size_t total = 0;
    // std::size_t reqLen = 0;
    // std::size_t nextReqLen = 0;
    // ssize_t recvRes = 0;  // signed size_t, can return -1
    // bool isBuffMaxed = false;

    // while (true)
    // {
    //     do
    //     {
    //         recvRes = recv(conn_fd, buffer + total,
    //                        REQ_BUFF_SIZE_REAL - total - 1, 0);
    //         total += recvRes;
    //         buffer[total] = '\0';  // for strchr
    //         reqEnd = std::strchr(buffer, '\n');
    //         isBuffMaxed = total >= REQ_BUFF_SIZE_REAL;
    //     } while (reqEnd == nullptr && recvRes && !isBuffMaxed);

    //     // exit loop, miner disconnected
    //     // res = 0, graceful disconnect
    //     // res = -1, error occured
    //     if (!recvRes || recvRes == -1)
    //     {
    //         stats_manager.PopWorker(cli_ptr->GetFullWorkerName(),
    //                                 cli_ptr->GetAddress());
    //         close(cli_ptr->GetSock());

    //         {
    //             std::scoped_lock cli_lock(clients_mutex);
    //             clients.erase(
    //                 std::find(clients.begin(), clients.end(), client));
    //         }

    //         Logger::Log(LogType::Info, LogField::Stratum,
    //                     "Client disconnected. res: {}, errno: {}", recvRes,
    //                     (int)errno);
    //         break;
    //     }
    //     // std::cout << total << std::endl;
    //     // std::cout << std::string_view(buffer, total) << std::endl;

    //     if (isBuffMaxed && !reqEnd)
    //     {
    //         Logger::Log(LogType::Critical, LogField::Stratum,
    //                     "Request too big. {}", buffer);

    //         total = 0;
    //         continue;
    //     }
    // }
}

void StratumServer::HandleReq(StratumClient *cli, WorkerContext *wc,
                              std::string_view req)
{
n    int id = 0;
    std::string_view method;
    simdjson::ondemand::array params;

    auto start = std::chrono::steady_clock::now();

    // std::cout << "last char -> " << (int)buffer[]
    simdjson::ondemand::document doc;
    try
    {
        doc = wc->json_parser.iterate(req.data(), req.size(),
                                      req.size() + simdjson::SIMDJSON_PADDING);

        simdjson::ondemand::object req = doc.get_object();
        id = static_cast<int>(req["id"].get_int64());
        method = req["method"].get_string();
        params = req["params"].get_array();
    }
    catch (const simdjson::simdjson_error &err)
    {
        SendReject(cli, id, (int)ShareCode::UNKNOWN, "Bad request");
        Logger::Log(LogType::Error, LogField::Stratum,
                    "Request JSON parse error: {}\nRequest: {}\n", err.what(),
                    req);
        return;
    }
    auto end = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    // std::cout << "req parse took: " << dur << "micro seconds." << std::endl;

    if (method == "mining.submit")
    {
        HandleSubmit(cli, wc, id, params);
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
        SendReject(cli, id, (int)ShareCode::UNKNOWN, "Unknown method");
        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Unknown request method: {}", method);
    }
}

void StratumServer::HandleBlockNotify()
{
    using namespace simdjson;
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
        for (auto &[sock, cli] : clients)
        {
            // (*it)->SetDifficulty(1000000, curtimeMs);
            // (*it)->SetDifficulty(job->GetTargetDiff(), curtimeMs);
            // UpdateDifficulty(*it);
            if (cli->GetIsPendingDiff())
            {
                cli->ActivatePendingDiff();
                UpdateDifficulty(cli.get());
            }
            BroadcastJob(cli.get(), newJob);
        }
        // TODO: reset shares
    }

    // in case of a crash without shares, we need to update round start time

    std::scoped_lock redis_lock(redis_mutex);

    // while (jobs.size() > 1)
    // {
    //     // TODO: fix
    //     //  delete jobs[0];
    //     //  jobs.erase();
    // }
    submission_manager.CheckImmatureSubmissions();
    redis_manager.AddNetworkHr(chain, curtimeMs, newJob->GetTargetDiff());

    redis_manager.SetMinerEffort(chain, ESTIMATED_EFFORT_KEY, "pow",
                                 newJob->GetEstimatedShares());
    // TODO: combine all redis functions one new block to one pipelined

    Benchmark<std::chrono::microseconds> ben("WORK GEN");

    Logger::Log(
        LogType::Info, LogField::JobManager,
        "Broadcasted new job: \n"
        "┌{0:─^{9}}┐\n"
        "│{1: ^{9}}│\n"
        "│{2: <{9}}│\n"
        "│{3: <{9}}│\n"
        "│{4: <{9}}│\n"
        "│{5: <{9}}│\n"
        "│{6: <{9}}│\n"
        "│{7: <{9}}│\n"
        "│{8: <{9}}│\n"
        "└{0:─^{9}}┘\n",
        "", fmt::format("Job #{}", newJob->GetId()),
        fmt::format("Height: {}", newJob->GetHeight()),
        fmt::format("Min time: {}", newJob->GetMinTime()),
        fmt::format("Difficulty: {}", newJob->GetTargetDiff()),
        fmt::format("Est. shares: {}", newJob->GetEstimatedShares()),
        fmt::format("Block reward: {}", newJob->GetBlockReward()),
        fmt::format("Transaction count: {}", newJob->GetTransactionCount()),
        fmt::format("Block size: {}", newJob->GetBlockSize()), 40);
}

// use wallletnotify with "%b %s %w" arg (block hash, txid, wallet address),
// check if block hash is smaller than current job's difficulty to check whether
// its pos block.
void StratumServer::HandleWalletNotify(WalletNotify *wal_notify)
{
    using namespace simdjson;
    std::string_view block_hash(wal_notify->block_hash,
                                sizeof(wal_notify->block_hash));
    std::string_view tx_id(wal_notify->txid, sizeof(wal_notify->txid));
    std::string_view address(wal_notify->wallet_address,
                             sizeof(wal_notify->wallet_address));

    auto bhash256 = UintToArith256(uint256S(block_hash.data()));
    double bhashDiff = BitsToDiff(bhash256.GetCompact());
    double pow_diff = job_manager.GetLastJob()->GetTargetDiff();

    Logger::Log(LogType::Info, LogField::Stratum,
                "Received PoS TxId: {}, block hash: {}", tx_id, block_hash);

    if (bhashDiff >= pow_diff)
    {
        return;  // pow block
    }

    if (address != coin_config.pool_addr)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "CheckAcceptedBlock error: Wrong address: {}, block: {}",
                    address, block_hash);
        return;
    }

    // now make sure we actually staked the PoS block and not just received a tx
    // inside one.

    std::string resBody;
    int64_t value;

    int resCode = daemon_manager.SendRpcReq<std::any>(
        resBody, 1, "getrawtransaction", std::any(tx_id),
        std::any(1));  // verboose

    if (resCode != 200)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "CheckAcceptedBlock error: Failed to getrawtransaction, "
                    "http code: %d",
                    resCode);
        return;
    }

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
    }
    catch (const simdjson_error &err)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "HandleWalletNotify: Failed to parse json, error: {}",
                    err.what());
        return;
    }

    // double check
    if (address != coin_config.pool_addr)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "CheckAcceptedBlock error: Wrong address: {}, block: {}",
                    address, block_hash);
    }

    resCode = daemon_manager.SendRpcReq<std::any>(resBody, 1, "getblock",
                                                  std::any(block_hash));

    if (resCode != 200)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "CheckAcceptedBlock error: Failed to getblock, "
                    "http code: {}",
                    resCode);
        return;
    }

    std::string_view validationType;
    std::string_view coinbaseTxId;
    int confirmations;
    uint32_t height;
    try
    {
        ondemand::document doc = httpParser.iterate(
            resBody.data(), resBody.size(), resBody.capacity());

        ondemand::object res = doc["result"].get_object();
        validationType = res["validationtype"];
        confirmations = res["confirmations"].get_int64();
        height = res["height"].get_int64();
        coinbaseTxId = (*res["tx"].get_array().begin()).get_string();
    }
    catch (const simdjson_error &err)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "HandleWalletNotify (1): Failed to parse json, error: {}",
                    err.what());
        return;
    }

    if (validationType != "stake")
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Double PoS block check failed! block hash: {}",
                    block_hash);
        return;
    }
    if (coinbaseTxId != tx_id)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "TxId is not coinbase, block hash: {}", block_hash);
        return;
    }
    // we have verified:
    //  block is PoS (twice),
    // the txid is ours (got its value),
    // the txid is indeed the coinbase tx

    Round round = round_manager_pos.GetChainRound(chain);
    const auto time = GetCurrentTimeMs();
    const int64_t duration_ms = time - round.round_start_ms;
    const double effort_percent =
        round.total_effort / round.estimated_effort * 100.f;  // TODO:

    auto submission = std::make_unique<BlockSubmission>(
        0, BlockType::POS, value, time, duration_ms, height,
        SubmissionManager::block_number, 0, effort_percent);

    memcpy(submission->chain, chain.data(), chain.size());
    memcpy(submission->miner, coin_config.pool_addr.data(), ADDRESS_LEN);
    memset(submission->worker, 0, sizeof(submission->worker));

    submission_manager.AddImmatureBlock(std::move(submission),
                                        coin_config.pos_fee);

    Logger::Log(LogType::Info, LogField::Stratum,
                "Added immature PoS Block! hash: {}", block_hash);
}

void StratumServer::HandleSubscribe(const StratumClient *cli, int id,
                                    simdjson::ondemand::array &params) const
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

    char response[128];
    int len =
        snprintf(response, sizeof(response),
                 "{\"id\":%d,\"result\":[null,\"%.*s\"],\"error\":null}\n", id,
                 EXTRANONCE_SIZE * 2, cli->GetExtraNonce().data());
    SendRaw(cli->GetSock(), response, len);

    Logger::Log(LogType::Info, LogField::Stratum, "client subscribed!");
}

void StratumServer::HandleAuthorize(StratumClient *cli, int id,
                                    simdjson::ondemand::array &params)
{
    using namespace simdjson;

    std::size_t split = 0;
    int resCode = 0;
    std::string valid_addr;
    std::string idTag = "null";
    std::string_view given_addr;
    std::string_view worker;
    bool isValid = false;
    bool isIdentity = false;

    std::string resultBody;
    ondemand::document doc;
    ondemand::object res;

    std::string_view worker_full;
    try
    {
        auto it = params.begin();
        worker_full = (*it).get_string();
    }
    catch (const simdjson_error &err)
    {
        Logger::Log(LogType::Error, LogField::Stratum,
                    "No worker name provided in authorization. err: {}",
                    err.what());
        return;
    }

    // worker name format: address.worker_name
    split = worker_full.find('.');

    if (split == std::string_view::npos)
    {
        SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                   "invalid worker name format, use: address/id@.worker");
        return;
    }
    else if (worker_full.size() > MAX_WORKER_NAME_LEN + ADDRESS_LEN + 1)
    {
        SendReject(
            cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
            "Worker name too long! (max " STRM(MAX_WORKER_NAME_LEN) " chars)");
        return;
    }

    given_addr = worker_full.substr(0, split);
    worker = worker_full.substr(split + 1, worker_full.size() - 1);

    bool oldAddress = redis_manager.DoesAddressExist(given_addr, valid_addr);

    if (!oldAddress)
    {
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

            std::string_view addr_sv = res["address"].get_string();
            valid_addr = std::string(addr_sv);
            isIdentity = valid_addr[0] == 'i';
        }
        catch (const simdjson_error &err)
        {
            SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                       "Server error: Failed to validate address!");
            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Authorize RPC (validateaddress) failed: {}",
                        err.what());
            return;
        }

        if (isIdentity)
        {
            if (given_addr == valid_addr)
            {
                // we were given an identity address (i not @), get the id@
                resCode = daemon_manager.SendRpcReq<std::any>(
                    resultBody, 1, "getidentity", std::any(valid_addr));

                try
                {
                    doc =
                        httpParser.iterate(resultBody.data(), resultBody.size(),
                                           resultBody.capacity());

                    res = doc["result"].get_object();
                    std::string_view id_sv = res["name"].get_string();
                    idTag = std::string(id_sv);
                }
                catch (const simdjson_error &err)
                {
                    SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                               "Server error: Failed to get id!");
                    Logger::Log(LogType::Critical, LogField::Stratum,
                                "Authorize RPC (getidentity) failed: {}",
                                err.what());
                    return;
                }
            }
            else
            {
                // we were given an id@
                idTag = std::string(given_addr);
            }
        }
    }

    std::string worker_full_str = fmt::format("{}.{}", valid_addr, worker);

    cli->SetAddress(worker_full_str, valid_addr);

    // string-views to non-local string
    bool added_to_db = stats_manager.AddWorker(
        cli->GetAddress(), cli->GetFullWorkerName(), idTag, std::time(nullptr));

    if (!added_to_db)
    {
        SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                   "Failed to add worker to database!");
        return;
    }
    cli->SetAuthorized();

    Logger::Log(LogType::Info, LogField::Stratum,
                "Authorized worker: {}, address: {}, id: {}", worker,
                valid_addr, idTag);

    SendAccept(cli, id);
    this->UpdateDifficulty(cli);

    const job_t *job = job_manager.GetLastJob();

    if (job == nullptr)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "No jobs to broadcast!");
        return;
    }

    this->BroadcastJob(cli, job);
}

// https://zips.z.cash/zip-0301#mining-submit
void StratumServer::HandleSubmit(StratumClient *cli, WorkerContext *wc, int id,
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
                    "Failed to parse submit: {}", err.what());
        return;
    }

    HandleShare(cli, wc, id, share);
}

void StratumServer::HandleShare(StratumClient *cli, WorkerContext *wc, int id,
                                Share &share)
{
    int64_t time = GetCurrentTimeMs();
    // Benchmark<std::chrono::microseconds> share_bench("Share process");
    auto start = TIME_NOW();

    ShareResult share_res;
    const job_t *job = job_manager.GetJob(share.jobId);

    ShareProcessor::Process(cli, wc, job, share, share_res, time);

    // > add share stats before submission to have accurate effort (its fast)
    // > possible that a timeseries wasn't created yet, so don't add shares
    if (share_res.code != ShareCode::UNAUTHORIZED_WORKER)
    {
        round_manager_pow.AddRoundShare(COIN_SYMBOL, cli->GetAddress(),
                                        share_res.difficulty);
        stats_manager.AddShare(cli->GetFullWorkerName(), cli->GetAddress(),
                               share_res.difficulty);
    }

    switch (share_res.code)
    {
        case ShareCode::VALID_BLOCK:
        {
            std::size_t blockSize = job->GetBlockSize();
            char blockData[blockSize];

            job->GetBlockHex(wc->block_header, blockData);

            // submit ASAP
            auto block_hex = std::string_view(blockData, blockSize);
            submission_manager.TrySubmit(chain, block_hex);

            const int confirmations = 0;
            const std::string_view worker_full(cli->GetFullWorkerName());
            const auto chainRound = round_manager_pow.GetChainRound(chain);
            const auto type = BlockType::POW;
            const int64_t duration_ms = time - chainRound.round_start_ms;
            const double effort_percent =
                (chainRound.total_effort / job->GetEstimatedShares()) * 100.f;

            auto submission = std::make_unique<BlockSubmission>(
                confirmations, type, job->GetBlockReward(), time, duration_ms,
                job->GetHeight(), SubmissionManager::block_number,
                share_res.difficulty, effort_percent);

            memcpy(submission->chain, chain.data(), chain.size());
            memcpy(submission->miner, worker_full.data(), ADDRESS_LEN);
            memcpy(submission->worker, worker_full.data() + ADDRESS_LEN + 1,
                   worker_full.size() - (ADDRESS_LEN + 1));
            Hexlify((char *)submission->hashHex, share_res.hash_bytes.data(),
                    HASH_SIZE);
            ReverseHex((char *)submission->hashHex, (char *)submission->hashHex,
                       HASH_SIZE_HEX);

            submission_manager.AddImmatureBlock(std::move(submission),
                                                coin_config.pow_fee);
            SendAccept(cli, id);
            break;
        }
        case ShareCode::VALID_SHARE:
            SendAccept(cli, id);
            break;
        case ShareCode::JOB_NOT_FOUND:
            Logger::Log(LogType::Warn, LogField::Stratum,
                        "Received share for unknown job id: {}", share.jobId);
        default:
            SendReject(cli, id, (int)share_res.code, share_res.message.c_str());
            break;
    }

    auto end = TIME_NOW();

    Logger::Log(
        LogType::Debug, LogField::Stratum,
        "Share processed in {}us, diff: {}, res: {}",
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count(),
        share_res.difficulty, (int)share_res.code);
}

void StratumServer::SendReject(const StratumClient *cli, int id, int err,
                               const char *msg) const
{
    char buffer[512];
    int len =
        snprintf(buffer, sizeof(buffer),
                 "{\"id\":%d,\"result\":null,\"error\":[%d,\"%s\",null]}\n", id,
                 err, msg);
    SendRaw(cli->GetSock(), buffer, len);
}

void StratumServer::SendAccept(const StratumClient *cli, int id) const
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

    char request[512];
    int len = snprintf(
        request, sizeof(request),
        "{\"id\":null,\"method\":\"mining.set_target\",\"params\":[\"%s\"]}\n",
        arith256.GetHex().c_str());
    // arith256.GetHex().c_str());

    SendRaw(cli->GetSock(), request, len);

    Logger::Log(LogType::Debug, LogField::Stratum,
                "Set difficulty for {} to {}", cli->GetFullWorkerName(),
                arith256.GetHex());
}

void StratumServer::BroadcastJob(const StratumClient *cli, const Job *job) const
{
    // auto res =
    auto notifyMsg = job->GetNotifyMessage();
    SendRaw(cli->GetSock(), notifyMsg.data(), notifyMsg.size());
}

void StratumServer::AddClient(int sockfd)
{
    std::unique_lock l(clients_mutex);
    auto client = std::make_unique<StratumClient>(sockfd, GetCurrentTimeMs(),
                                                  coin_config.default_diff);

    clients.emplace(sockfd, std::move(client));
}

StratumClient *StratumServer::GetClient(int sockfd)
{
    std::unique_lock l(clients_mutex);
    return clients[sockfd].get();
}