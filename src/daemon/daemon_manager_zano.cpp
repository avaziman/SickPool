#include "daemon_manager_zano.hpp"

using namespace simdjson;
using namespace std::string_view_literals;

bool DaemonManagerZano::GetBlockTemplate(BlockTemplateResCn& templateRes,
                                         std::string_view addr,
                                         std::string_view extra_data,
                                         simdjson::ondemand::parser& parser)
{
    std::string result_body;
    auto obj = {std::make_pair("wallet_address"sv, addr),
                std::make_pair("extra_text"sv, extra_data)};

    int res_code = SendRpcReq(result_body, 1, "getblocktemplate"sv,
                              DaemonRpc::ToJsonStr(obj), "GET /json_rpc");

    if (res_code != 200)
    {
        Logger::Log(LogType::Warn, LogField::DaemonManager,
                    "Failed to getblocktemplate, rescode: {}, response: {}",
                    res_code, result_body);
        return false;
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
        Logger::Log(LogType::Warn, LogField::DaemonManager,
                    "Failed to parse getblocktemplate response: {}, {}",
                    err.what(), result_body);
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
        Logger::Log(LogType::Warn, LogField::DaemonManager,
                    "Failed to submitblock, rescode: {}, response: {}",
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
        Logger::Log(LogType::Warn, LogField::DaemonManager,
                    "Failed to parse submitblock response: {}, {}",
                    err.what(), result_body);
        return false;
    }

    return true;
}
