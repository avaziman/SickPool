#include "stratum_server_zec.hpp"

#include "static_config.hpp"

template <StaticConf confs>
void StratumServerZec<confs>::HandleReq(Connection<StratumClient> *conn,
                                        WorkerContextT *wc,
                                        std::string_view req)
{
    int id = 0;
    const int sock = conn->sock;
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
            "Request JSON parse error: {}\nRequest: {}\n", err.what(), req);
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
            UpdateDifficulty(cli);

            const JobT *job = this->job_manager.GetLastJob();

            if (job == nullptr)
            {
                logger.Log<LogType::Critical>("No jobs to broadcast!");
                return;
            }

            BroadcastJob(cli, job);
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

// https://zips.z.cash/zip-0301#mining-subscribe
template <StaticConf confs>
RpcResult StratumServerZec<confs>::HandleSubscribe(
    StratumClient *cli, simdjson::ondemand::array &params) const
{
    // Mining software info format: "SickMiner/6.9"
    // REQ
    //{"id": 1, "method": "mining.subscribe", "params": ["MINER_USER_AGENT",
    //"SESSION_ID", "CONNECT_HOST", CONNECT_PORT]} \n

    // RES
    //{"id": 1, "result": ["SESSION_ID", "NONCE_1"], "error": null} \n

    // we don't send session id

    logger.Log<LogType::Info>("client subscribed!");

    return RpcResult(ResCode::OK,
                     fmt::format("[null,\"{}\"]", cli->extra_nonce_sv));
}

// https://zips.z.cash/zip-0301#mining-authorize
// template <StaticConf confs>
// RpcResult StratumServerZec<confs>::HandleAuthorize(
//     StratumClient *cli, simdjson::ondemand::array &params)
// {
//     using namespace simdjson;

//     std::size_t split = 0;
//     int resCode = 0;
//     std::string id_tag = "null";
//     std::string_view given_addr;
//     std::string_view worker;
//     bool isIdentity = false;

//     std::string_view worker_full;
//     try
//     {
//         worker_full = params.at(0).get_string();
//     }
//     catch (const simdjson_error &err)
//     {
//         logger.Log<LogType::Error>(
//             "No worker name provided in authorization. err: {}", err.what());

//         return RpcResult(ResCode::UNAUTHORIZED_WORKER, "Bad request");
//     }

//     // worker name format: address.worker_name
//     split = worker_full.find('.');

//     if (split == std::string_view::npos)
//     {
//         return RpcResult(ResCode::UNAUTHORIZED_WORKER,
//                          "invalid worker name format, use: address/id@.worker");
//     }
//     else if (worker_full.size() > MAX_WORKER_NAME_LEN + ADDRESS_LEN + 1)
//     {
//         return RpcResult(
//             ResCode::UNAUTHORIZED_WORKER,
//             "Worker name too long! (max " STRM(MAX_WORKER_NAME_LEN) " chars)");
//     }

//     given_addr = worker_full.substr(0, split);
//     worker = worker_full.substr(split + 1, worker_full.size() - 1);
//     ValidateAddressRes va_res;

//     if (!daemon_manager.ValidateAddress(va_res, httpParser, given_addr))
//     {
//         return RpcResult(ResCode::UNAUTHORIZED_WORKER,
//                          "Failed to validate address!");
//     }

//     if (!va_res.is_valid)
//     {
//         return RpcResult(ResCode::UNAUTHORIZED_WORKER,
//                          fmt::format("Invalid address {}!", given_addr));
//     }

//     isIdentity = va_res.valid_addr[0] == 'i';

//     if (isIdentity)
//     {
//         if (given_addr == va_res.valid_addr)
//         {
//             // we were given an identity address (i not @), get the id@
//             GetIdentityRes id_res;
//             if (!daemon_manager.GetIdentity(id_res, httpParser, given_addr))
//             {
//                 logger.Log<LogType::Critical>(
//                     "Authorize RPC (getidentity) failed!");

//                 return RpcResult(
//                     ResCode::UNAUTHORIZED_WORKER,
//                     fmt::format("Server error: Failed to get id! {}",
//                                 given_addr));
//             }

//             id_tag = id_res.name;
//         }
//         else
//         {
//             // we were given an id@
//             id_tag = std::string(given_addr);
//         }
//     }
//     // }

//     std::string worker_full_str =
//         fmt::format("{}.{}", va_res.valid_addr, worker);

//     cli->SetAddress(worker_full_str, va_res.valid_addr);

//     // string-views to non-local string
//     bool added_to_db = stats_manager.AddWorker(
//         cli->GetAddress(), cli->GetFullWorkerName(), va_res.script_pub_key,
//         std::time(nullptr), id_tag, coin_config.min_payout_threshold);

//     if (!added_to_db)
//     {
//         return RpcResult(ResCode::UNAUTHORIZED_WORKER,
//                          "Failed to add worker to database!");
//     }
//     cli->SetAuthorized();

//     logger.Log<LogType::Info>("Authorized worker: {}, address: {}, id: {}",
//                               worker, va_res.valid_addr, id_tag);

//     return RpcResult(ResCode::OK);
// }

// https://zips.z.cash/zip-0301#mining-submit
template <StaticConf confs>
RpcResult StratumServerZec<confs>::HandleSubmit(
    StratumClient *cli, WorkerContextT *wc, simdjson::ondemand::array &params)
{
    using namespace simdjson;
    using namespace CoinConstantsZec;

    // parsing takes 0-1 us
    ShareZec share;
    std::string parse_error = "";

    const auto end = params.end();
    auto it = params.begin();
    error_code error;

    if (it == end || (error = (*it).get_string().get(share.worker)))
    {
        parse_error = "Bad worker.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.job_id)) ||
             share.job_id.size() != JOBID_SIZE * 2)
    {
        parse_error = "Bad job id.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.time_sv)) ||
             share.time_sv.size() != sizeof(share.time) * 2)
    {
        parse_error = "Bad time.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.nonce2)) ||
             share.nonce2.size() != EXTRANONCE2_SIZE * 2)
    {
        parse_error = "Bad nonce2.";
    }
    else if (++it == end || (error = (*it).get_string().get(share.solution)) ||
             share.solution.size() !=
                 (SOLUTION_SIZE + SOLUTION_LENGTH_SIZE) * 2)
    {
        parse_error = "Bad solution.";
    }
    share.time = HexToUint(share.time_sv.data(), sizeof(share.time) * 2);

    if (!parse_error.empty())
    {
        logger.Log<LogType::Critical>("Failed to parse submit: {}",
                                      parse_error);
        return RpcResult(ResCode::UNKNOWN, parse_error);
    }

    return HandleShare(cli, wc, share);
}

template <StaticConf confs>
void StratumServerZec<confs>::UpdateDifficulty(
    Connection<StratumClient> *conn)
{
    auto diff_hex = GetDifficultyHex<confs>(conn->ptr->GetDifficulty());
    std::string_view hex_target_sv(diff_hex.data(), diff_hex.size());

    std::string msg = fmt::format(
        "{{\"id\":null,\"method\":\"mining.set_"
        "difficulty\",\"params\":[\"{}\"]}}\n",
        hex_target_sv);

    this->SendRaw(conn->sock, msg.data(), msg.size());

    logger.Log<LogType::Debug>("Set difficulty for {} to {}",
                               conn->ptr->GetFullWorkerName(), hex_target_sv);
}