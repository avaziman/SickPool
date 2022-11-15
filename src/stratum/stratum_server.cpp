#include "stratum_server.hpp"
template class StratumServer<ZanoStatic>;

template <StaticConf confs>
StratumServer<confs>::StratumServer(CoinConfig &&conf)
    : StratumBase(std::move(conf)),
      daemon_manager(coin_config.rpcs),
      payment_manager(&redis_manager, &daemon_manager, coin_config.pool_addr,
                      coin_config.payment_interval_seconds,
                      coin_config.min_payout_threshold),
      job_manager(&daemon_manager, &payment_manager, coin_config.pool_addr),
      block_submitter(&redis_manager, &daemon_manager, &round_manager),
      stats_manager(redis_manager, &diff_manager, &round_manager, &conf.stats)
{
    static_assert(confs.DIFF1 != 0, "DIFF1 can't be zero!");

    if (auto error =
            httpParser.allocate(HTTP_REQ_ALLOCATE, MAX_HTTP_JSON_DEPTH);
        error != simdjson::SUCCESS)
    {
        logger.Log<LogType::Critical>(
            "Failed to allocate http parser buffer: {}",
            simdjson::error_message(error));
        exit(EXIT_FAILURE);
    }

    // init hash functions if needed
    HashWrapper::InitSHA256();

    if constexpr (confs.HASH_ALGO == HashAlgo::VERUSHASH_V2b2)
    {
        HashWrapper::InitVerusHash();
    }

    stats_thread = std::jthread(
        std::bind_front(&StatsManager::Start<confs>, &stats_manager));
}

template <StaticConf confs>
StratumServer<confs>::~StratumServer()
{
    stats_thread.request_stop();
    this->logger.Log<LogType::Info>("Stratum destroyed.");
}

// TODO: test buffer too little
// TODO: test buffer flooded
template <StaticConf confs>
void StratumServer<confs>::HandleBlockNotify()
{
    int64_t curtime_ms = GetCurrentTimeMs();

    const job_t *new_job = job_manager.GetNewJob();

    while (new_job == nullptr)
    {
        new_job = job_manager.GetNewJob();

        logger.Log<LogType::Critical>(
            "Block update error: Failed to generate new job! retrying...");

        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        if (this->GetStopToken().stop_requested())
        {
            return;
        }
    }

    // save it to process round
    {
        std::shared_lock clients_read_lock(clients_mutex);
        for (const auto &[conn, _] : clients)
        {
            auto cli = conn->ptr.get();
            if (cli->GetIsAuthorized())
            {
                if (cli->GetIsPendingDiff())
                {
                    cli->ActivatePendingDiff();
                    UpdateDifficulty(conn.get());
                }

                BroadcastJob(conn.get(), new_job);
            }
        }

        // after we broadcasted new job:
        // > kick clients with below min difficulty
        // > update the map according to difficulties (notify best miners first)
        // > reset duplicate shares map

        for (auto it = clients.begin(); it != clients.end();)
        {
            const auto cli = (*it).first->ptr;
            const auto cli_diff = cli->GetDifficulty();
            if (cli_diff < coin_config.minimum_difficulty)
            {
                this->DisconnectClient(it->first);
                it++;
            }
            else
            {
                ++it;
            }

            cli->ResetShareSet();
            // update the difficulty in the map
            it->second = cli_diff;
        }
        // TODO: reset shares
    }

    // payment_manager.UpdatePayouts(&round_manager, curtime_ms);

    // the estimated share amount is supposed to be meet at block time
    const double net_est_hr = new_job->expected_hashes / confs.BLOCK_TIME;
    round_manager.netwrok_hr = net_est_hr;
    round_manager.difficulty = new_job->target_diff;

    // redis_manager.SetNewBlockStats(coin_config.symbol, curtime_ms, net_est_hr,
    //                                new_job->target_diff);

    logger.Log<LogType::Info>(
        "Broadcasted new job: \n"
        "┌{0:─^{8}}┐\n"
        "│{1: ^{8}}│\n"
        "│{2: <{8}}│\n"
        "│{3: <{8}}│\n"
        "│{4: <{8}}│\n"
        "│{5: <{8}}│\n"
        "│{6: <{8}}│\n"
        "│{7: <{8}}│\n"
        // "│{8: <{9}}│\n"
        // "└{0:─^{9}}┘\n",
        "└{0:─^{8}}┘\n",
        "", fmt::format("Job #{}", new_job->id),
        fmt::format("Height: {}", new_job->height),
        // fmt::format("Min time: {}", new_job->min_time),
        fmt::format("Difficulty: {}", new_job->target_diff),
        fmt::format("Est. shares: {}", new_job->expected_hashes),
        fmt::format("Block reward: {}", new_job->coinbase_value),
        fmt::format("Transaction count: {}", new_job->tx_count),
        fmt::format("Block size: {}", new_job->block_size * 2), new_job->id.size() + 10);
}

template <StaticConf confs>
RpcResult StratumServer<confs>::HandleShare(StratumClient *cli,
                                            WorkerContextT *wc,
                                            ShareType &share)
{
    int64_t time = GetCurrentTimeMs();
    // Benchmark<std::chrono::microseconds> share_bench("Share process");
    auto start = TIME_NOW();

    ShareResult share_res;
    RpcResult rpc_res(ResCode::OK);
    const job_t *job = job_manager.GetJob(share.jobId);
    // to make sure the job isn't removed while we are using it,
    // and at the same time allow multiple threads to use same job
    std::shared_lock<std::shared_mutex> job_read_lock;

    if (job == nullptr)
    {
        share_res.code = ResCode::JOB_NOT_FOUND;
        share_res.message = "Job not found";
        share_res.difficulty = static_cast<double>(BadDiff::STALE_SHARE_DIFF);
    }
    else
    {
        job_read_lock = std::shared_lock<std::shared_mutex>(job->job_mutex);
        ShareProcessor::Process<confs>(share_res, cli, wc, job, share, time);
    }

    if (share_res.code == ResCode::VALID_BLOCK) [[unlikely]]
    {
        std::size_t blockSize = job->block_size * 2;
        std::string blockData;
        blockData.reserve(blockSize);

#ifdef STRATUM_PROTOCOL_ZEC
        job->GetBlockHex(blockData, wc->block_header);
#elif defined(STRATUM_PROTOCOL_BTC)
        job->GetBlockHex(blockData, wc->block_header, cli->extra_nonce_sv,
                         share.extranonce2);
#elif defined(STRATUM_PROTOCOL_CN)
        job_read_lock.unlock();

        // TODO: find way to upgrade to exclusive lock
        // may be straved here
        std::unique_lock job_write(job->job_mutex);
        job->GetBlockHex(blockData, share.nonce);
#else

#endif

#ifndef STRATUM_PROTOCOL_CN
        if (job->is_payment)
        {
            payment_manager.finished_payment.reset(
                payment_manager.pending_payment.release());
        }
#endif

        // submit ASAP
        auto block_hex = std::string_view(blockData.data(), blockSize);
        block_submitter.TrySubmit(coin_config.symbol, block_hex, httpParser);

        logger.Log<LogType::Info>(
            "Block hex: {}", std::string_view(blockData.data(), blockSize));

        // job will remain safe thanks to the lock.
        const std::string_view worker_full(cli->GetFullWorkerName());
        const auto chain_round = round_manager.GetChainRound();
        const uint64_t duration_ms = time - chain_round.round_start_ms;
        const BlockType type = BlockType::POW;
#ifndef STRATUM_PROTOCOL_CN
        if (job->is_payment)
        {
            type = BlockType::POW_PAYMENT;
        }
#else

#endif
        const auto dur = time - chain_round.round_start_ms;
        const double effort_percent =
            ((chain_round.total_effort - share_res.difficulty) /
             job->target_diff) *
            100.f;

#if HASH_ALGO == HASH_ALGO_X25X
        HashWrapper::X22I(share_res.hash_bytes.data(), wc->block_header);
        // std::reverse(share_res.hash_bytes.begin(),
        //              share_res.hash_bytes.end());
#endif
        std::array<char, 64> block_hash_hex;
        auto bs = BlockSubmission{.confirmations = 0,
                                  .block_type = BlockType::POW,
                                  .block_reward = job->coinbase_value,
                                  .time_ms = time,
                                  .duration_ms = duration_ms,
                                  .height = job->height,
                                  .number = redis_manager.GetBlockNumber(),
                                  .difficulty = share_res.difficulty,
                                  .effort_percent = effort_percent,
                                  .chain = 0,
                                  .miner_id = cli->GetId().miner_id.id,
                                  .worker_id = cli->GetId().worker_id.id,
                                  .hash_hex = block_hash_hex};

        auto submission = std::make_unique<BlockSubmission>(bs);

        block_submitter.AddImmatureBlock(std::move(submission),
                                         coin_config.pow_fee);
    }
    else if (share_res.code == ResCode::VALID_SHARE) [[likely]]
    {
        round_manager.AddRoundShare(cli->GetId().miner_id, share_res.difficulty);
    }
    else
    {
        // logger.Log<LogType::Warn>(
        //             "Received bad share for job id: {}", share.jobId);

        rpc_res = RpcResult(share_res.code, share_res.message);
    }

    stats_manager.AddShare(cli->GetId(),
                           share_res.difficulty);

    auto end = TIME_NOW();

    // logger.Log<
    //     LogType::Debug>(
    //     "Share processed in {}us, diff: {}, res: {}",
    //     std::chrono::duration_cast<std::chrono::microseconds>(end - start)
    //         .count(),
    //     share_res.difficulty, (int)share_res.code);

    return rpc_res;
}

// the shared pointer makes sure the client won't be freed as long as we are
// processing it

template <StaticConf confs>
void StratumServer<confs>::HandleConsumeable(connection_it *it)
{
    static thread_local WorkerContext<confs.BLOCK_HEADER_SIZE> wc;

    std::shared_ptr<Connection<StratumClient>> conn = *(*it);

    // bigger than 0
    size_t req_len = 0;
    size_t next_req_len = 0;
    const char *last_req_end = nullptr;
    const char *req_start = nullptr;
    char *req_end = nullptr;
    char *buffer = conn->req_buff;

    req_end = std::strchr(buffer, '\n');

    // there can be multiple messages in 1 recv
    // {1}\n{2}\n
    // strchr(buffer, '{');  // "should" be first char
    req_start = &buffer[0];
    while (req_end)
    {
        req_len = req_end - req_start;
        HandleReq(conn.get(), &wc, std::string_view(req_start, req_len));

        last_req_end = req_end;
        req_start = req_end + 1;
        req_end = std::strchr(req_end + 1, '\n');
    }

    // if we haven't received a full request then don't touch the buffer
    if (last_req_end)
    {
        next_req_len =
            conn->req_pos - (last_req_end - buffer + 1);  // don't inlucde \n

        std::memmove(buffer, last_req_end + 1, next_req_len);
        buffer[next_req_len] = '\0';

        conn->req_pos = next_req_len;
        last_req_end = nullptr;
    }
}

template <StaticConf confs>
bool StratumServer<confs>::HandleConnected(connection_it *it)
{
    std::shared_ptr<Connection<StratumClient>> conn = *(*it);

    conn->ptr = std::make_shared<StratumClient>(GetCurrentTimeMs(),
                                                coin_config.default_difficulty);

    if (job_manager.GetLastJob() == nullptr)
    {
        // disconnect if we don't have any jobs to not cause a crash, more
        // efficient to check here than everytime we broadcast a job (there will
        // not be case where job is empty after the first job.)
        logger.Log<LogType::Warn>(
            "Rejecting client connection, as there isn't a job!");
        return false;
    }

    std::unique_lock lock(clients_mutex);
    clients.try_emplace(conn, 0);

    return true;
}

template <StaticConf confs>
void StratumServer<confs>::DisconnectClient(
    const std::shared_ptr<Connection<StratumClient>> conn_ptr)
{
    auto sock = conn_ptr->sock;
    stats_manager.PopWorker(conn_ptr->ptr->GetId());
    std::unique_lock lock(clients_mutex);
    clients.erase(conn_ptr);

    logger.Log<LogType::Info>("Stratum client disconnected. sock: {}", sock);
}