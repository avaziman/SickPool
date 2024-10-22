#include "stratum_server_cn.hpp"

#include "static_config.hpp"

template class StratumServerCn<ZanoStatic>;

template <StaticConf confs>
void StratumServerCn<confs>::HandleReq(Connection<StratumClient> *conn,
                                       WorkerContextT *wc, std::string_view req)
{
    using namespace std::string_view_literals;
    int64_t id = 0;
    const int sock = conn->sockfd;
    const auto cli = conn->ptr.get();

    std::string_view worker;
    std::string_view method;
    simdjson::ondemand::array params;

    bool is_submit_work = false;
    bool is_login = false;

    // std::cout << "last char -> " << (int)buffer[]
    simdjson::ondemand::document doc;
    try
    {
        doc = wc->json_parser.iterate(req.data(), req.size(),
                                      req.size() + simdjson::SIMDJSON_PADDING);

        simdjson::ondemand::object req_ob = doc.get_object();
        id = req_ob["id"].get_int64();
        method = req_ob["method"].get_string();

        is_submit_work = method == "eth_submitWork"sv;
        is_login = !is_submit_work && method == "eth_submitLogin"sv;

        if (is_submit_work || is_login)
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

    RpcResult res(ResCode::UNKNOWN);

    if (is_submit_work)
    {
        res = HandleSubmit(conn, wc, params, worker);
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
        logger.template Log<LogType::Warn>("Unknown request method: {}",
                                           method);
    }

    this->SendRes(sock, id, res);
}

template <StaticConf confs>
RpcResult StratumServerCn<confs>::HandleSubmit(
    Connection<StratumClient> *con, WorkerContextT *wc,
    simdjson::ondemand::array &params, std::string_view worker)
{
    using namespace simdjson;
    using namespace std::string_view_literals;

    // parsing takes 0-1 us
    ShareCn share;
    share.worker = worker;

    std::string_view nonce_sv;
    std::array<Field, 3> fields{{
        Field{"nonce"sv, &nonce_sv, sizeof(share.nonce) * 2 + 2},
        Field{"header pow"sv, &share.job_id,
              confs.BLOCK_HASH_SIZE * 2 + 2},
        Field{"mix digest"sv, &share.mix_digest,
              confs.BLOCK_HASH_SIZE * 2 + 2},
    }};

    if (std::string parse_err = ParseShareParams(fields, params);
        !parse_err.empty())
    {
        return RpcResult(ResCode::UNKNOWN,
                         "Failed to parse share: " + parse_err);
    }

    share.job_id =
        share.job_id.substr(2);  // remove the hex prefix as we don't save it.
    nonce_sv = nonce_sv.substr(2);

    std::from_chars(nonce_sv.data(), nonce_sv.data() + nonce_sv.size(),
                    share.nonce, 16);
    share.nonce = bswap_64(share.nonce);

    return this->HandleShare(con, wc, share);
}

template <StaticConf confs>
void StratumServerCn<confs>::BroadcastJob(Connection<StratumClient> *conn,
                                          const JobT *job, int64_t id) const
{
    std::string msg =
        job->template GetWorkMessage<confs>(conn->ptr->GetDifficulty(), id);
    this->SendRaw(conn->sockfd, msg);
}

template <StaticConf confs>
void StratumServerCn<confs>::BroadcastJob(Connection<StratumClient> *conn,
                                          double diff, const JobT *job) const
{
    std::string msg = job->template GetWorkMessage<confs>(diff);
    this->SendRaw(conn->sockfd, msg);
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