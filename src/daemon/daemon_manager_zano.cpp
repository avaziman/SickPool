#include "daemon_manager_zano.hpp"

using namespace simdjson;
using namespace std::string_view_literals;

bool DaemonManagerZano::GetBlockTemplate(BlockTemplateResCn& templateRes,
                                         std::string_view addr,
                                         std::string_view extra_data,
                                         simdjson::ondemand::parser& parser)
{
    std::string result_body;

    int res_code = SendRpcReq(
        result_body, 1, "getblocktemplate"sv,
        DaemonRpc::ToJsonStr(std::make_pair("wallet_address"sv, addr),
                             std::make_pair("extra_text"sv, extra_data)),
        "GET /json_rpc");

    if (res_code != 200)
    {
        logger.Log<LogType::Error>(
            "Failed to getblocktemplate, rescode: {}, response: {}",
                        res_code, result_body);
    }

    ondemand::object res;
    try
    {
        templateRes.doc = parser.iterate(result_body.data(), result_body.size(),
                                         result_body.capacity());

        res = templateRes.doc["result"].get_object();

        templateRes.blob = res["blocktemplate_blob"].get_string();
        // templateRes.difficulty = res["difficulty"].get_int64();
        // given as string
        std::string_view diffsv = res["difficulty"].get_string();
        std::from_chars(diffsv.data(), diffsv.data() + diffsv.size(),
                        templateRes.difficulty);
                        
        templateRes.height = res["height"].get_int64();
        templateRes.prev_hash = res["prev_hash"].get_string();
        templateRes.seed = res["seed"].get_string();
    }
    catch (const simdjson_error& err)
    {
        logger.Log<LogType::Error>(
            "Failed to parse signrawtransaction response: {}", err.what());
        return false;
    }

    return true;
}

bool DaemonManagerZano::SubmitBlock(std::string_view block_hex,
                                         simdjson::ondemand::parser& parser)
{
    std::string result_body;

    int res_code = SendRpcReq(result_body, 1, "submitblock"sv,
                              DaemonRpc::GetParamsStr(block_hex), "POST /json_rpc");

    if (res_code != 200)
    {
        logger.Log<LogType::Error>(
            "Failed to getblocktemplate, rescode: {}, response: {}",
                        res_code, result_body);
        return false;
    }

    ondemand::object res;
    try
    {
        auto doc = parser.iterate(result_body.data(), result_body.size(),
                                         result_body.capacity());

        res = doc["result"].get_object();
    }
    catch (const simdjson_error& err)
    {
        // logger.Log<LogType::Error>fmt::format(
        //     "Failed to parse signrawtransaction response: {}", err.what()));
        logger.Log<LogType::Error>(
            "Failed to parse signrawtransaction response: {}", err.what());
        return false;
    }

    return true;
}

bool DaemonManagerZano::Transfer(TransferResCn& transfer_res, const std::vector<std::pair<std::string, int64_t>>& dests, int64_t fee, simdjson::ondemand::parser& parser){
    std::string result_body;

    std::vector<std::string> dest_strs;
    dest_strs.reserve(dests.size());

    for (const auto& dest : dests)
    {
        dest_strs.push_back(DaemonRpc::ToJsonStr(std::make_pair("address"sv, dest.first), std::make_pair("amount"sv, dest.second)));
    }
    std::string params = DaemonRpc::ToJsonStr(
        std::make_pair("destinations"sv, DaemonRpc::GetParamsStr(dest_strs)), std::make_pair("fee"sv, fee), std::make_pair("mixin"sv, 0));

    int res_code =
        SendRpcReq(result_body, 1, "transfer"sv,
                   params, "POST /json_rpc"sv);

    if (res_code != 200)
    {
        logger.Log<LogType::Error>(
            "Failed to getblocktemplate, rescode: {}, response: {}",
                        res_code, result_body);
        return false;
    }

    ondemand::object res;
    try
    {
        auto doc = parser.iterate(result_body.data(), result_body.size(),
                                  result_body.capacity());

        res = doc["result"].get_object();
        transfer_res.txid = res["tx_hash"].get_string();
        transfer_res.tx_size = res["tx_size"].get_int64();
    }
    catch (const simdjson_error& err)
    {
        // logger.Log<LogType::Error>fmt::format(
        //     "Failed to parse signrawtransaction response: {}", err.what()));
        logger.Log<LogType::Error>(
            "Failed to parse transfer response: {}", err.what());
        return false;
    }

    return true;
}
