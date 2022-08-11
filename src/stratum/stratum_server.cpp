#include "stratum_server.hpp"

StratumServer::StratumServer(const CoinConfig &conf)
    : coin_config(conf),
      redis_manager("127.0.0.1", (int)conf.redis_port),
      diff_manager(&clients_mutex, coin_config.target_shares_rate),
      payment_manager(coin_config.pool_addr,
                      coin_config.payment_interval_seconds,
                      coin_config.min_payout_threshold),
      round_manager_pow(&redis_manager, "pow"),
      round_manager_pos(&redis_manager, "pos"),
      stats_manager(&redis_manager, &diff_manager, &round_manager_pow,
                    (int)coin_config.hashrate_interval_seconds,
                    (int)coin_config.effort_interval_seconds,
                    (int)coin_config.average_hashrate_interval_seconds,
                    (int)coin_config.diff_adjust_seconds,
                    (int)coin_config.hashrate_ttl_seconds),
      daemon_manager(coin_config.rpcs),
      job_manager(&daemon_manager, &payment_manager, coin_config.pool_addr),
      submission_manager(&redis_manager, &daemon_manager, &payment_manager,
                         &round_manager_pow, &round_manager_pos)
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

    stats_thread =
        std::jthread(std::bind_front(&StatsManager::Start, &stats_manager));
    stats_thread.detach();

    control_server.Start(coin_config.control_port);
    control_thread = std::jthread(
        std::bind_front(&StratumServer::HandleControlCommands, this));
    control_thread.detach();
}

StratumServer::~StratumServer()
{
    std::scoped_lock l(clients_mutex);
    for (auto &it : clients)
    {
        close(it->sock);
    }
    close(listening_fd);
    close(epoll_fd);

    Logger::Log(LogType::Info, LogField::Stratum,
                "Stratum destroyed. Connections closed.");
}

void StratumServer::HandleControlCommands(std::stop_token st)
{
    char buff[256] = {0};
    while (!st.stop_requested())
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
        {
            // format: %b%s%w (block hash, txid, wallet address)
            WalletNotify *wallet_notify =
                reinterpret_cast<WalletNotify *>(buff + 2);
            HandleWalletNotify(wallet_notify);
            break;
        }
        default:
            Logger::Log(LogType::Warn, LogField::StatsManager,
                        "Unknown control command {} received.", (int)cmd);
            break;
    }
}

void StratumServer::Listen()
{
    Logger::Log(LogType::Info, LogField::Stratum,
                "Started listenning on port: {}", coin_config.stratum_port);

    epoll_fd = epoll_create1(0);

    if (epoll_fd == -1)
    {
        throw std::runtime_error(fmt::format(
            "Failed to create epoll: {} -> {}.", errno, std::strerror(errno)));
    }

    // throws if goes wrong
    listening_fd = CreateListeningSock();

    if (listen(listening_fd, MAX_CONNECTIONS) == -1)
        throw std::runtime_error(
            "Stratum server failed to enter listenning state.");

    const auto worker_amount = std::thread::hardware_concurrency();
    processing_threads.reserve(worker_amount);

    for (auto i = 0; i < worker_amount; i++)
    {
        processing_threads.emplace_back(
            std::bind_front(&StratumServer::ServiceSockets, this));
    }

    HandleBlockNotify();

    for (auto &t : processing_threads)
    {
        t.join();
    }
    stats_thread.join();
    control_thread.join();
}

void StratumServer::Stop()
{
    stats_thread.request_stop();
    control_thread.request_stop();
    for (auto &t : processing_threads)
    {
        t.request_stop();
    }
}

void StratumServer::ServiceSockets(std::stop_token st)
{
    struct epoll_event events[MAX_CONNECTION_EVENTS];

    WorkerContext wc;
    int conn_fd;
    int epoll_res;
    int serviced_fd;
    ssize_t recv_res;

    int64_t total_microseconds = 0;
    int64_t total_requests = 0;

    Logger::Log(LogType::Info, LogField::Stratum,
                "Starting servicing sockets on thread {}", gettid());

    while (!st.stop_requested())
    {
        epoll_res =
            epoll_wait(epoll_fd, events, MAX_CONNECTION_EVENTS, EPOLL_TIMEOUT);

        if (epoll_res == -1)
        {
            if (errno == EBADF || errno == EFAULT || errno == EINVAL)
            {
                Logger::Log(LogType::Error, LogField::Stratum,
                            "Failed to epoll_wait: {} -> {}", errno,
                            std::strerror(errno));
                return;
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

            if (serviced_fd == listening_fd)
            {
                HandleNewConnection();
            }
            else
            {
                auto start = TIME_NOW();
                total_requests++;

                // new data ready to be read
                std::list<std::unique_ptr<StratumClient>>::iterator *cli_it =
                    (std::list<std::unique_ptr<StratumClient>>::iterator *)
                        events[i]
                            .data.ptr;
                const auto sockfd = (*(*cli_it))->sock;
                auto flags = events[i].events;

                if (flags & EPOLLERR)
                {
                    int error = 0;
                    socklen_t errlen = sizeof(error);
                    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void *)&error,
                                   &errlen) == 0)
                    {
                        Logger::Log(LogType::Warn, LogField::Stratum,
                                    "Received epoll error on socket fd {}, "
                                    "errno: {} -> {}",
                                    sockfd, error, std::strerror(error));
                    }
                    // EraseClient(cli_node->sock, cli_node->it);
                    // continue;
                }

                HandleReadableSocket(cli_it, &wc);

                auto end = TIME_NOW();

                total_microseconds +=
                    std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                          start)
                        .count();
            }
        }
    }

    Logger::Log(LogType::Info, LogField::Stratum,
                "Average req processing took: {}",
                (double)total_microseconds / total_requests);
}

void StratumServer::HandleNewConnection()
{
    struct sockaddr_in conn_addr;
    socklen_t addr_len = sizeof(conn_addr);

    // for valgrind :)
    int conn_fd = AcceptConnection(&conn_addr, &addr_len);

    if (conn_fd < 0)
    {
        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Failed to accept socket to errno: {} "
                    "-> errno: {}. ",
                    conn_fd, errno);
        return;
    }

    char ip_str[INET_ADDRSTRLEN];
    struct in_addr ip_addr = conn_addr.sin_addr;
    inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));

    auto ip = std::string(ip_str);

    // add client before added to epoll so that we can lock and
    // avoid data race
    std::list<std::unique_ptr<StratumClient>>::iterator *cli_node =
        AddClient(conn_fd, ip);

    // since this is a union only one member can be assigned
    struct epoll_event conn_ev
    {
        .events = EPOLLIN | EPOLLET | EPOLLONESHOT, .data = {
            .ptr = cli_node,
        }
    };

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &conn_ev) == -1)
    {
        EraseClient(conn_fd, cli_node);

        Logger::Log(
            LogType::Warn, LogField::Stratum,
            "Failed to add socket of client with ip {} to epoll list errno: {} "
            "-> errno: {}. ",
            ip, conn_fd, errno);
        return;
    }

    Logger::Log(LogType::Info, LogField::Stratum,
                "Tcp client connected, ip: {}, sock {}", ip, conn_fd);
}

void StratumServer::HandleReadableSocket(
    std::list<std::unique_ptr<StratumClient>>::iterator *it, WorkerContext *wc)
{
    StratumClient *cli = (*(*it)).get();
    const auto sockfd = cli->sock;

    // bigger than 0
    ssize_t recv_res = 1;
    size_t req_len = 0;
    size_t next_req_len = 0;
    const char *last_req_end = nullptr;
    const char *req_start = nullptr;
    char *req_end = nullptr;
    char *buffer = cli->req_buff;

    std::string ip(cli->GetIp());
    while (true)
    {
        recv_res = recv(sockfd, cli->req_buff + cli->req_pos,
                        REQ_BUFF_SIZE_REAL - cli->req_pos - 1, 0);
        // make copy so we can print after erasing client

        if (recv_res == -1)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                break;
            }

            EraseClient(sockfd, it);

            Logger::Log(
                LogType::Warn, LogField::Stratum,
                "Client with ip {} disconnected because of socket (fd: {}) "
                "error: {} -> {}.",
                ip, sockfd, errno, std::strerror(errno));
            return;
        }
        else if (recv_res == 0)
        {
            // should happened on flooded buffer
            EraseClient(sockfd, it);

            Logger::Log(LogType::Info, LogField::Stratum,
                        "Client with ip {} (sock {}) disconnected.", ip,
                        sockfd);
            return;
        }

        cli->req_pos += recv_res;
        buffer[cli->req_pos] = '\0';  // for strchr
        req_end = std::strchr(buffer, '\n');

        // there can be multiple messages in 1 recv
        // {1}\n{2}\n
        // strchr(buffer, '{');  // "should" be first char
        req_start = &buffer[0];
        while (req_end)
        {
            req_len = req_end - req_start;
            HandleReq(cli, wc, std::string_view(req_start, req_len));

            last_req_end = req_end;
            req_start = req_end + 1;
            req_end = std::strchr(req_end + 1, '\n');
        }

        // if we haven't received a full request then don't touch the buffer
        if (last_req_end)
        {
            next_req_len =
                cli->req_pos - (last_req_end - buffer + 1);  // don't inlucde \n

            std::memmove(buffer, last_req_end + 1, next_req_len);
            buffer[next_req_len] = '\0';

            cli->req_pos = next_req_len;
            last_req_end = nullptr;
        }
    }

    struct epoll_event conn_ev
    {
        .events = EPOLLIN | EPOLLET | EPOLLONESHOT, .data = {.ptr = it }
    };

    // rearm socket in epoll interest list
    // ~1us
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, cli->sock, &conn_ev) == -1)
    {
        EraseClient(sockfd, it);

        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Failed to rearm socket (fd: {}) of client with ip {} to"
                    "epoll list errno: {} "
                    "-> errno: {}. ",
                    sockfd, ip, errno, std::strerror(errno));
        return;
    }
}
// TODO: test buffer too little
// TODO: test buffer flooded

int StratumServer::CreateListeningSock()
{
    int optval = 1;

    listening_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listening_fd == -1)
        throw std::runtime_error("Failed to create stratum socket");

    if (setsockopt(listening_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
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

    if (bind(listening_fd, (const sockaddr *)&addr, sizeof(addr)) == -1)
    {
        throw std::runtime_error(
            fmt::format("Stratum server failed to bind to port: {}",
                        coin_config.stratum_port));
    }

    struct epoll_event listener_ev;
    memset(&listener_ev, 0, sizeof(listener_ev));
    listener_ev.events = EPOLLIN | EPOLLET;
    listener_ev.data.fd = listening_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listening_fd, &listener_ev) == -1)
    {
        throw std::runtime_error(
            fmt::format("Failed to add listener socket to epoll set: {} -> {}",
                        errno, std::strerror(errno)));
    }

    return listening_fd;
}

int StratumServer::AcceptConnection(sockaddr_in *addr, socklen_t *addr_size)
{
    int flags = O_NONBLOCK;
    int conn_fd = accept4(listening_fd, (sockaddr *)addr, addr_size, flags);

    if (conn_fd == -1)
    {
        return -1;
    }

    int yes = 1;
    if (setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1)
    {
        return -2;
    }
    return conn_fd;
}

void StratumServer::HandleReq(StratumClient *cli, WorkerContext *wc,
                              std::string_view req)
{
    int id = 0;
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
    int64_t curtime_ms = GetCurrentTimeMs();

    const job_t *new_job = job_manager.GetNewJob();

    auto st = control_thread.get_stop_token();

    while (new_job == nullptr && !st.stop_requested())
    {
        new_job = job_manager.GetNewJob();

        Logger::Log(
            LogType::Critical, LogField::Stratum,
            "Block update error: Failed to generate new job! retrying...");

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    if (st.stop_requested())
    {
        return;
    }

    // save it to process round
    {
        std::scoped_lock clients_lock(clients_mutex);
        for (auto &it : clients)
        {
            auto cli = it.get();
            if (cli->GetIsPendingDiff())
            {
                cli->ActivatePendingDiff();
                UpdateDifficulty(cli);
            }
            BroadcastJob(cli, new_job);
        }
        // TODO: reset shares
    }

    payment_manager.UpdatePayouts(&round_manager_pow, curtime_ms);

    std::scoped_lock redis_lock(redis_mutex);

    // while (jobs.size() > 1)
    // {
    //     // TODO: fix
    //     //  delete jobs[0];
    //     //  jobs.erase();
    // }
    submission_manager.CheckImmatureSubmissions();
    redis_manager.AddNetworkHr(chain, curtime_ms, new_job->GetTargetDiff());

    // TODO: think how to set estimted effort
    //  redis_manager.SetMinerEffort(chain, RedisManager::ESTIMATED_EFFORT_KEY,
    //  "pow",
    //                               newJob->GetEstimatedShares());
    //  TODO: combine all redis functions one new block to one pipelined

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
        "", fmt::format("Job #{}", new_job->GetId()),
        fmt::format("Height: {}", new_job->GetHeight()),
        fmt::format("Min time: {}", new_job->GetMinTime()),
        fmt::format("Difficulty: {}", new_job->GetTargetDiff()),
        fmt::format("Est. shares: {}", new_job->GetEstimatedShares()),
        fmt::format("Block reward: {}", new_job->GetBlockReward()),
        fmt::format("Transaction count: {}", new_job->GetTransactionCount()),
        fmt::format("Block size: {}", new_job->GetBlockSize()), 40);
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
    int64_t reward_value;

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
        reward_value = output1["valueSat"].get_int64();

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

    BlockRes block_res;
    bool getblock_res =
        daemon_manager.GetBlock(block_res, httpParser, block_hash);

    if (!getblock_res)
    {
        Logger::Log(LogType::Error, LogField::Stratum, "Failed to getblock {}!",
                    block_hash);
        return;
    }

    if (block_res.validation_type != ValidationType::STAKE)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Double PoS block check failed! block hash: {}",
                    block_hash);
        return;
    }
    if (!block_res.tx_ids.size() || block_res.tx_ids[0] != tx_id)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "TxId is not coinbase, block hash: {}", block_hash);
        return;
    }
    // we have verified:
    //  block is PoS (twice),
    // the txid is ours (got its value),
    // the txid is indeed the coinbase tx

    Round round = round_manager_pos.GetChainRound();
    const auto now_ms = GetCurrentTimeMs();
    const double effort_percent =
        round.total_effort / round.estimated_effort * 100.f;

    uint8_t block_hash_bin[HASH_SIZE];
    uint8_t tx_id_bin[HASH_SIZE];

    Unhexlify(block_hash_bin, block_hash.data(), block_hash.size());
    Unhexlify(tx_id_bin, tx_id.data(), tx_id.size());

    auto submission = std::make_unique<ExtendedSubmission>(
        std::string_view(chain), std::string_view(coin_config.pool_addr),
        BlockType::POS, block_res.height, reward_value, round, now_ms,
        SubmissionManager::block_number, 0.d, 0.d, block_hash_bin, tx_id_bin);

    submission_manager.AddImmatureBlock(std::move(submission),
                                        coin_config.pos_fee);

    Logger::Log(LogType::Info, LogField::Stratum,
                "Added immature PoS Block! hash: {}", block_hash);
}

void StratumServer::HandleSubscribe(StratumClient *cli, int id,
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
    SendRaw(cli->sock, response, len);

    Logger::Log(LogType::Info, LogField::Stratum, "client subscribed!");
}

void StratumServer::HandleAuthorize(StratumClient *cli, int id,
                                    simdjson::ondemand::array &params)
{
    using namespace simdjson;

    std::size_t split = 0;
    int resCode = 0;
    std::string id_tag = "null";
    std::string_view given_addr;
    std::string_view worker;
    bool isIdentity = false;

    std::string_view worker_full;
    try
    {
        // O(n = length of array)
        // crashes (assertion) if we try to increment iterator and its out of
        // range
        worker_full = params.at(0).get_string();
    }
    catch (const simdjson_error &err)
    {
        SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER, "Bad request");

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
    ValidateAddressRes va_res;

    // bool oldAddress = redis_manager.DoesAddressExist(given_addr,
    // va_res.valid_addr);

    // if (!oldAddress)
    // {
    if (!daemon_manager.ValidateAddress(va_res, httpParser, given_addr))
    {
        SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                   "Failed to validate address!");
        return;
    }

    if (!va_res.is_valid)
    {
        SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                   fmt::format("Invalid address {}!", given_addr).c_str());
        return;
    }

    isIdentity = va_res.valid_addr[0] == 'i';

    if (isIdentity)
    {
        if (given_addr == va_res.valid_addr)
        {
            // we were given an identity address (i not @), get the id@
            GetIdentityRes id_res;
            if (!daemon_manager.GetIdentity(id_res, httpParser, given_addr))
            {
                SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                           "Server error: Failed to get id!");
                Logger::Log(LogType::Critical, LogField::Stratum,
                            "Authorize RPC (getidentity) failed: {}");
                return;
            }

            id_tag = id_res.name;
        }
        else
        {
            // we were given an id@
            id_tag = std::string(given_addr);
        }
    }
    // }

    std::string worker_full_str =
        fmt::format("{}.{}", va_res.valid_addr, worker);

    cli->SetAddress(worker_full_str, va_res.valid_addr);

    // string-views to non-local string
    bool added_to_db = stats_manager.AddWorker(
        cli->GetAddress(), cli->GetFullWorkerName(), id_tag,
        va_res.script_pub_key, std::time(nullptr));

    if (!added_to_db)
    {
        SendReject(cli, id, (int)ShareCode::UNAUTHORIZED_WORKER,
                   "Failed to add worker to database!");
        return;
    }
    cli->SetAuthorized();

    Logger::Log(LogType::Info, LogField::Stratum,
                "Authorized worker: {}, address: {}, id: {}", worker,
                va_res.valid_addr, id_tag);

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
    using namespace simdjson;
    // parsing takes 0-1 us
    Share share;
    const char *parse_error = nullptr;

    const auto end = params.end();
    auto it = params.begin();
    error_code error;

    if (it == end || (error = (*it).get_string().get(share.worker)))
    {
        parse_error = "Bad worker.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.jobId)) ||
             share.jobId.size() != sizeof(uint32_t) * 2)
    {
        parse_error = "Bad job id.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.time)) ||
             share.time.size() != sizeof(uint32_t) * 2)
    {
        parse_error = "Bad time.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.nonce2)) ||
             share.nonce2.size() != NONCE2_SIZE * 2)
    {
        parse_error = "Bad nonce2.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.solution)) ||
             share.solution.size() !=
                 (SOLUTION_SIZE + SOLUTION_LENGTH_SIZE) * 2)
    {
        parse_error = "Bad solution.";
    }

    if (parse_error)
    {
        SendReject(cli, id, (int)ShareCode::UNKNOWN, parse_error);
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Failed to parse submit: {}", parse_error);
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
        round_manager_pow.AddRoundShare(cli->GetAddress(),
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

            const std::string_view worker_full(cli->GetFullWorkerName());
            const auto chainRound = round_manager_pow.GetChainRound();
            const auto type =
                job->GetIsPayment() ? BlockType::POW_PAYMENT : BlockType::POW;
            const double effort_percent =
                (chainRound.total_effort / job->GetEstimatedShares()) * 100.f;

            auto submission = std::make_unique<ExtendedSubmission>(
                chain, worker_full, type, job->GetHeight(),
                job->GetBlockReward(), chainRound, time,
                SubmissionManager::block_number,
                share_res.difficulty, effort_percent,
                share_res.hash_bytes.data(), job->coinbase_tx_id);

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

    // Logger::Log(
    //     LogType::Debug, LogField::Stratum,
    //     "Share processed in {}us, diff: {}, res: {}",
    //     std::chrono::duration_cast<std::chrono::microseconds>(end - start)
    //         .count(),
    //     share_res.difficulty, (int)share_res.code);
}

void StratumServer::SendReject(StratumClient *cli, int id, int err,
                               const char *msg) const
{
    char buffer[512];
    int len =
        snprintf(buffer, sizeof(buffer),
                 "{\"id\":%d,\"result\":null,\"error\":[%d,\"%s\",null]}\n", id,
                 err, msg);
    SendRaw(cli->sock, buffer, len);
}

void StratumServer::SendAccept(StratumClient *cli, int id) const
{
    char buff[512];
    int len = snprintf(buff, sizeof(buff),
                       "{\"id\":%d,\"result\":true,\"error\":null}\n", id);
    SendRaw(cli->sock, buff, len);
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

    SendRaw(cli->sock, request, len);

    Logger::Log(LogType::Debug, LogField::Stratum,
                "Set difficulty for {} to {}", cli->GetFullWorkerName(),
                arith256.GetHex());
}

void StratumServer::BroadcastJob(StratumClient *cli, const Job *job) const
{
    // auto res =
    auto notifyMsg = job->GetNotifyMessage();
    SendRaw(cli->sock, notifyMsg.data(), notifyMsg.size());
}

std::list<std::unique_ptr<StratumClient>>::iterator *StratumServer::AddClient(
    int sockfd, const std::string &ip)
{
    std::unique_lock l(clients_mutex);

    auto client = std::make_unique<StratumClient>(
        sockfd, ip, GetCurrentTimeMs(), coin_config.default_diff);
    clients.emplace_back(std::move(client));

    auto it = clients.end();
    --it;
    clients.back()->it = it;

    return &clients.back()->it;
}

void StratumServer::EraseClient(
    int sockfd, std::list<std::unique_ptr<StratumClient>>::iterator *it)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, nullptr) == -1)
    {
        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Failed to remove socket {} from epoll list errno: {} "
                    "-> errno: {}. ",
                    sockfd, errno, std::strerror(errno));
    }
    if (close(sockfd) == -1)
    {
        Logger::Log(LogType::Warn, LogField::Stratum,
                    "Failed to close socket {} errno: {} "
                    "-> errno: {}. ",
                    sockfd, errno, std::strerror(errno));
    }

    std::unique_lock l(clients_mutex);
    clients.erase(*it);
}

// TODO: switch unordered map to linked list
// TODO: add EPOLLERR, EPOLLHUP