#include "stratum_server_btc.hpp"

#include "static_config.hpp"

template <StaticConf confs>
void StratumServerBtc<confs>::BroadcastJob(Connection<StratumClient> *conn,
                                           const JobT *job) const
{
    auto notifyMsg = job->GetNotifyMessage();
    SendRaw(conn->sockfd, notifyMsg.data(), notifyMsg.size());
}

template <StaticConf confs>
void StratumServerBtc<confs>::HandleReq(Connection<StratumClient> *conn,
                                        WorkerContextT *wc,
                                        std::string_view req)
{
    using namespace std::string_view_literals;
    int64_t id = 0;
    const int sock = conn->sockfd;
    // safe to remove from list
    auto cli = conn->ptr.get();

    std::string_view method;
    simdjson::ondemand::array params;

    simdjson::ondemand::document doc;
    try
    {
        doc = wc->json_parser.iterate(req.data(), req.size(),
                                      req.size() + simdjson::SIMDJSON_PADDING);

        simdjson::ondemand::object req_obj = doc.get_object();
        id = req_obj["id"].get_int64();
        method = req_obj["method"].get_string();
        params = req_obj["params"].get_array();
    }
    catch (const simdjson::simdjson_error &err)
    {
        this->SendRes(sock, id, RpcResult(ResCode::UNKNOWN, "Bad request"));
        logger.Log<LogType::Error>(
            "Request JSON parse error: {}\nRequest: {}\n", err.what(), req);
        return;
    }

    RpcResult res(ResCode::UNKNOWN);

    if (method == "mining.submit"sv)
    {
        res = HandleSubmit(cli, wc, params);
    }
    else if (method == "mining.subscribe"sv)
    {
        res = HandleSubscribe(cli, params);
    }
    else if (method == "mining.authorize"sv)
    {
        res = this->HandleAuthorize(cli, params);
        if (res.code == ResCode::OK)
        {
            this->SendRes(sock, id, res);
            UpdateDifficulty(conn);

            const JobT *job = this->job_manager.GetLastJob();

            if (job == nullptr)
            {
                logger.Log<LogType::Critical>("No jobs to broadcast!");
                return;
            }

            BroadcastJob(conn, job, conn);
            return;
        }
    }
    else
    {
        res = RpcResult(ResCode::UNKNOWN, "Unknown method");
        logger.Log<LogType::Warn>("Unknown request method: {}", method);
    }

    this->SendRes(sock, id, res);
}

// https://en.bitcoin.it/wiki/Stratum_mining_protocol#mining.submit
template <StaticConf confs>
RpcResult StratumServerBtc<confs>::HandleSubscribe(
    StratumClient *cli, simdjson::ondemand::array &params) const
{
    using namespace CoinConstantsBtc;

    logger.Log<LogType::Info>("client subscribed!");

    // [[["mining.set_difficulty", "subscription id 1"], ["mining.notify",
    // "subscription id 2"]], "extranonce1", extranonce2_size]

    // null subscription ids
    auto res = fmt::format(
        "[[[\"mining.set_difficulty\",null],[\"mining.notify\",null]],\"{}\",{}]",
        cli->extra_nonce_sv, EXTRANONCE2_SIZE);

    return RpcResult(ResCode::OK, res);
}

// https://en.bitcoin.it/wiki/Stratum_mining_protocol#mining.submit
template <StaticConf confs>
RpcResult StratumServerBtc<confs>::HandleSubmit(
    StratumClient *cli, WorkerContextT *wc, simdjson::ondemand::array &params)
{
    using namespace std::string_view_literals;
    using namespace simdjson;
    using namespace CoinConstantsBtc;

    // parsing takes 0-1 us
    ShareBtc share;

    std::string_view nonce_sv;
    std::string_view time_sv;

    std::array<Field, 5> fields{{
        Field{"worker"sv, &share.worker, 0},
        Field{"job id"sv, &share.job_id, this->JOBID_SIZE * 2},
        Field{"nonce2"sv, &share.extranonce2, EXTRANONCE2_SIZE * 2},
        Field{"time"sv, &time_sv, sizeof(share.time) * 2},
        Field{"nonce"sv, &nonce_sv, sizeof(share.nonce) * 2},
    }};
    share.time = HexToUint(time_sv.data(), sizeof(share.time) * 2);
    share.nonce = HexToUint(nonce_sv.data(), sizeof(share.nonce) * 2);

    if (std::string parse_err = ParseShareParams(fields, params);
        !parse_err.empty())
    {
        return RpcResult(ResCode::UNKNOWN,
                         "Failed to parse share: " + parse_err);
    }

    return HandleShare(cli, wc, share);
}

// https://en.bitcoin.it/wiki/Stratum_mining_protocol#mining.set_difficulty
template <StaticConf confs>
void StratumServerBtc<confs>::UpdateDifficulty(Connection<StratumClient> *conn)
{
    const auto cli = conn->ptr;

    std::string req = fmt::format(
        "{{\"id\":null,\"method\":\"mining.set_"
        "difficulty\",\"params\":[{}]}}\n",
        cli->GetDifficulty());

    this->SendRaw(conn->sockfd, req.data(), req.size());

    logger.Log<LogType::Debug>("Set difficulty for {} to {}", cli->GetDifficulty());
}