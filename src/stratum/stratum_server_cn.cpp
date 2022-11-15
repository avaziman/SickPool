#include "static_config.hpp"

#ifdef STRATUM_PROTOCOL_CN
#include "stratum_server_cn.hpp"

template class StratumServerCn<ZanoStatic>;

template <StaticConf confs>
void StratumServerCn<confs>::HandleReq(Connection<StratumClient> *conn,
                                       WorkerContextT *wc, std::string_view req)
{
    int id = 0;
    const int sock = conn->sock;
    const auto cli = conn->ptr.get();

    std::string_view worker;
    std::string_view method;
    simdjson::ondemand::array params;

    auto start = std::chrono::steady_clock::now();

    bool is_submit_work = false;
    bool is_login = false;

    // std::cout << "last char -> " << (int)buffer[]
    simdjson::ondemand::document doc;
    try
    {
        doc = wc->json_parser.iterate(req.data(), req.size(),
                                      req.size() + simdjson::SIMDJSON_PADDING);

        simdjson::ondemand::object req_ob = doc.get_object();
        id = static_cast<int>(req_ob["id"].get_int64());
        method = req_ob["method"].get_string();

        if ((is_submit_work = method == "eth_submitWork") ||
            (is_login = method == "eth_submitLogin"))
        {
            worker = req_ob["worker"].get_string();
        }

        params = req_ob["params"].get_array();
    }
    catch (const simdjson::simdjson_error &err)
    {
        this->SendRes(sock, id, RpcResult(ResCode::UNKNOWN, "Bad request"));
        logger.Log<LogType::Error>(
            "Request JSON parse error: {}\nRequest: {}\n", err.what(), req);
        return;
    }
    auto end = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    // std::cout << "req parse took: " << dur << "micro seconds." << std::endl;

    RpcResult res(ResCode::UNKNOWN);

    // eth_submitWork
    if (is_submit_work)
    {
        res = HandleSubmit(cli, wc, params, worker);
    }
    else if (method == "eth_getWork")
    {
        this->BroadcastJob(conn, this->job_manager.GetLastJob(), id);
    }
    // eth_submitLogin
    else if (is_login)
    {
        res = HandleAuthorize(cli, params, worker);
    }
    else
    {
        res = RpcResult(ResCode::UNKNOWN, "Unknown method");
        logger.Log<LogType::Warn>("Unknown request method: {}", method);
    }

    this->SendRes(sock, id, res);
}

template <StaticConf confs>
RpcResult StratumServerCn<confs>::HandleSubmit(
    StratumClient *cli, WorkerContextT *wc, simdjson::ondemand::array &params,
    std::string_view worker)
{
    using namespace simdjson;
    // parsing takes 0-1 us
    ShareCn share;
    share.worker = worker;
    std::string parse_error = "";

    const auto end = params.end();
    auto it = params.begin();
    error_code error;

    if (!cli->GetIsAuthorized())
    {
        return RpcResult(ResCode::UNAUTHORIZED_WORKER, "Unauthorized worker");
    }

    if (it == end || (error = (*it).get_string().get(share.nonce_sv)) ||
        share.nonce_sv.size() != sizeof(share.nonce) * 2 + 2)
    {
        parse_error = "Bad nonce.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.jobId)) ||
             share.jobId.size() != confs.BLOCK_HASH_SIZE * 2 + 2)
    {
        parse_error = "Bad header pow hash.";
    }
    else if (++it == end ||
             (error = (*it).get_string().get(share.mix_digest)) ||
             share.mix_digest.size() != confs.BLOCK_HASH_SIZE * 2 + 2)
    {
        parse_error = "Bad mix digest.";
    }

    share.nonce = HexToUint(share.nonce_sv.data() + 2, sizeof(share.nonce) * 2);
    share.jobId =
        share.jobId.substr(2);  // remove the hex prefix as we don't save it.

    if (!parse_error.empty())
    {
        logger.Log<LogType::Critical>("Failed to parse submit: {}",
                                      parse_error);
        return RpcResult(ResCode::UNKNOWN, parse_error);
    }

    return this->HandleShare(cli, wc, share);
}

template <StaticConf confs>
RpcResult StratumServerCn<confs>::HandleAuthorize(
    StratumClient *cli, simdjson::ondemand::array &params,
    std::string_view worker)
{
    using namespace simdjson;

    std::string_view given_addr;
    bool isIdentity = false;

    try
    {
        given_addr = params.at(0).get_string();
    }
    catch (const simdjson_error &err)
    {
        this->logger.Log<LogType::Error>(
            "No address provided in authorization. err: {}", err.what());

        return RpcResult(ResCode::UNAUTHORIZED_WORKER, "Bad request");
    }

    if (worker.size() > MAX_WORKER_NAME_LEN)
    {
        return RpcResult(
            ResCode::UNAUTHORIZED_WORKER,
            "Worker name too long! (max " xSTRR(MAX_WORKER_NAME_LEN) " chars)");
    }

    currency::blobdata addr_blob =
        std::string(given_addr.data(), given_addr.size());
    uint64_t prefix;
    currency::blobdata addr_data;
    if (!tools::base58::decode_addr(addr_blob, prefix, addr_data))
    {
        return RpcResult(ResCode::UNAUTHORIZED_WORKER,
                         fmt::format("Invalid address {}!", given_addr));
    }

    auto addr_encoded = tools::base58::encode(addr_data);


    // string-views to non-local string
    WorkerFullId worker_full_id(0, 0);
    bool added_to_db = this->stats_manager.AddWorker(
        worker_full_id, given_addr, worker, GetCurrentTimeMs(), "",
        this->coin_config.min_payout_threshold);

    if (!added_to_db)
    {
        return RpcResult(ResCode::UNAUTHORIZED_WORKER,
                         "Failed to add worker to database!");
    }
    std::string worker_full_str = fmt::format("{}.{}", given_addr, worker);
    cli->SetAuthorized(worker_full_id, std::move(worker_full_str));

    logger.Log<LogType::Info>("Authorized worker: {}, address: {}", worker,
                              given_addr);

    return RpcResult(ResCode::OK);
}

template <StaticConf confs>
void StratumServerCn<confs>::BroadcastJob(Connection<StratumClient> *conn,
                                          const job_t *job, int id) const
{
    std::string msg = job->template GetWorkMessage<confs>(conn->ptr.get(), id);
    this->SendRaw(conn->sock, msg.data(), msg.size());
}

template <StaticConf confs>
void StratumServerCn<confs>::BroadcastJob(Connection<StratumClient> *conn,
                                          const job_t *job) const
{
    std::string msg = job->template GetWorkMessage<confs>(conn->ptr.get());
    this->SendRaw(conn->sock, msg.data(), msg.size());
}

template <StaticConf confs>
// template <typename S = confs.STRATUM_PROTOCOL>
// typename std::enable_if_t<confs.STRATUM_PROTOCOL == StratumProtocol::CN,
// void>
void StratumServerCn<confs>::UpdateDifficulty(Connection<StratumClient> *conn)
{
    // nothing to be done; as it's sent on new job.
}

#endif