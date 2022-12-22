#include "stratum_server_cn.hpp"

#include "static_config.hpp"

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
        logger.template Log<LogType::Error>(
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
        std::shared_ptr<JobCryptoNote> last_job =
            this->job_manager.GetLastJob();
        this->BroadcastJob(conn, last_job.get(), id);
    }
    // eth_submitLogin
    else if (is_login)
    {
        res = HandleAuthorize(cli, params, worker);
    }
    else
    {
        res = RpcResult(ResCode::UNKNOWN, "Unknown method");
        logger.template Log<LogType::Warn>("Unknown request method: {}", method);
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
    else if (++it == end || (error = (*it).get_string().get(share.job_id)) ||
             share.job_id.size() != confs.BLOCK_HASH_SIZE * 2 + 2)
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
    share.job_id =
        share.job_id.substr(2);  // remove the hex prefix as we don't save it.

    if (!parse_error.empty())
    {
        logger.template Log<LogType::Critical>("Failed to parse submit: {}",
                                      parse_error);
        return RpcResult(ResCode::UNKNOWN, parse_error);
    }

    return this->HandleShare(cli, wc, share);
}
template <StaticConf confs>
void StratumServerCn<confs>::BroadcastJob(Connection<StratumClient> *conn,
                                          const JobT *job, int id) const
{
    std::string msg = job->template GetWorkMessage<confs>(conn->ptr.get(), id);
    this->SendRaw(conn->sock, msg.data(), msg.size());
}

template <StaticConf confs>
void StratumServerCn<confs>::BroadcastJob(Connection<StratumClient> *conn,
                                          const JobT *job) const
{
    std::string msg = job->template GetWorkMessage<confs>(conn->ptr.get());
    this->SendRaw(conn->sock, msg.data(), msg.size());
}

template <StaticConf confs>
void StratumServerCn<confs>::UpdateDifficulty(Connection<StratumClient> *conn)
{
    // nothing to be done; as it's sent on new job.
}

template <StaticConf confs>
RpcResult StratumServerCn<confs>::HandleAuthorize(
    StratumClient *cli, simdjson::ondemand::array &params,
    std::string_view worker)
{
    using namespace simdjson;

    std::string_view miner;
    try
    {
        miner = params.at(0).get_string();
    }
    catch (const simdjson_error &err)
    {
        logger.template Log<LogType::Error>(
            "No miner name provided in authorization. err: {}", err.what());

        return RpcResult(ResCode::UNAUTHORIZED_WORKER, "Bad request");
    }

    return StratumServer<confs>::HandleAuthorize(cli, miner, worker);
}