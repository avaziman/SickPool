#include "static_config.hpp"
#include "stratum_server_btc.hpp"

template <StaticConf confs>
void StratumServerBtc<confs>::BroadcastJob(Connection<StratumClient> *conn,
                                 const JobT *job) const
{
    auto notifyMsg = job->GetNotifyMessage();
    SendRaw(conn->sockfd, notifyMsg.data(), notifyMsg.size());
}

template <StaticConf confs>
void StratumServerBtc<confs>::HandleReq(Connection<StratumClient>* conn, WorkerContextT *wc,
                                 std::string_view req)
{
    int id = 0;
    const int sock = conn->sockfd;
    const auto cli = conn->ptr.get();

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
        this->SendRes(sock, id, RpcResult(ResCode::UNKNOWN, "Bad request"));
        logger.Log<LogType::Error>(
                    "Request JSON parse error: {}\nRequest: {}\n", err.what(),
                    req);
        return;
    }
    auto end = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    // std::cout << "req parse took: " << dur << "micro seconds." << std::endl;

    RpcResult res(ResCode::UNKNOWN);

    if (method == "mining.submit")
    {
        res = HandleSubmit(cli, wc, params);
    }
    else if (method == "mining.subscribe")
    {
        res = HandleSubscribe(cli, params);
    }
    else if (method == "mining.authorize")
    {
        res = this->HandleAuthorize(cli, params);
        if (res.code == ResCode::OK)
        {
            this->SendRes(sock, id, res);
            UpdateDifficulty(conn);

            const JobT *job = this->job_manager.GetLastJob();

            if (job == nullptr)
            {
                logger.Log<LogType::Critical>( 
                            "No jobs to broadcast!");
                return;
            }

            BroadcastJob(conn, job);
            return;
        }
    }
    else
    {
        res = RpcResult(ResCode::UNKNOWN, "Unknown method");
        logger.Log<LogType::Warn>(
                    "Unknown request method: {}", method);
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
        "[[[\"mining.set_difficulty\",null],"
        "[\"mining.notify\",null]],\"{}\",{}]",
        cli->extra_nonce_sv, EXTRANONCE2_SIZE);

    return RpcResult(ResCode::OK, res);
}

// https://en.bitcoin.it/wiki/Stratum_mining_protocol#mining.submit
template <StaticConf confs>
RpcResult StratumServerBtc<confs>::HandleSubmit(StratumClient *cli, WorkerContextT *wc,
                                         simdjson::ondemand::array &params)
{
    using namespace simdjson;
    using namespace CoinConstantsBtc;

    // parsing takes 0-1 us
    ShareBtc share;
    std::string parse_error = "";

    const auto end = params.end();
    auto it = params.begin();
    error_code error;

    if (!cli->GetIsAuthorized())
    {
        return RpcResult(ResCode::UNAUTHORIZED_WORKER, "Unauthorized worker");
    }

    if (it == end || (error = (*it).get_string().get(share.worker)))
    {
        parse_error = "Bad worker.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.job_id)) ||
             share.job_id.size() != JOBID_SIZE * 2)
    {
        parse_error = "Bad job id.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.extranonce2)) ||
             share.extranonce2.size() != EXTRANONCE2_SIZE * 2)
    {
        parse_error = "Bad nonce2.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.time_sv)) ||
             share.time_sv.size() != sizeof(share.time) * 2)
    {
        parse_error = "Bad time.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.nonce_sv)) ||
             share.nonce_sv.size() != sizeof(share.nonce) * 2)
    {
        parse_error = "Bad nonce.";
    }
    share.time = HexToUint(share.time_sv.data(), sizeof(share.time) * 2);
    share.nonce = HexToUint(share.nonce_sv.data(), sizeof(share.nonce) * 2);

    if (!parse_error.empty())
    {
        logger.Log<LogType::Critical>( 
                    "Failed to parse submit: {}", parse_error);
        return RpcResult(ResCode::UNKNOWN, parse_error);
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

    logger.Log<LogType::Debug>( 
                "Set difficulty for {} to {}", cli->GetFullWorkerName(),
                cli->GetDifficulty());
}