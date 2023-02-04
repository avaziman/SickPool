#include "daemon_manager_zano.hpp"

using namespace simdjson;
using namespace std::string_view_literals;

bool DaemonManagerT<Coin::ZANO>::GetBlockTemplate(
    BlockTemplateRes& templateRes, std::string_view addr,
    std::string_view extra_data, simdjson::ondemand::parser& parser)
{
    std::string result_body;

    const auto method = "getblocktemplate"sv;

    if (int res_code = SendRpcReq(
            result_body, 1, method,
            DaemonRpc::ToJsonObj(std::make_pair("wallet_address"sv, addr),
                                 std::make_pair("extra_text"sv, extra_data)),
            "GET /json_rpc");
        res_code != 200)
    {
        LOG_CODE_ERR(method, res_code, result_body);
        return false;
    }

    ondemand::object res;
    try
    {
        templateRes.doc = parser.iterate(result_body.data(), result_body.size(),
                                         result_body.capacity());

        res = templateRes.doc["result"].get_object();

        templateRes.blob = res["blocktemplate_blob"].get_string();

        // given as string
        std::string_view diffsv = res["difficulty"].get_string();
        std::from_chars(diffsv.data(), diffsv.data() + diffsv.size(),
                        templateRes.difficulty);

        templateRes.height = static_cast<uint32_t>(res["height"].get_int64());
        templateRes.prev_hash = res["prev_hash"].get_string();
        templateRes.seed = res["seed"].get_string();
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}

bool DaemonManagerT<Coin::ZANO>::SubmitBlock(std::string_view block_hex,
                                    simdjson::ondemand::parser& parser)
{
    std::string result_body;

    const auto method = "submitblock"sv;

    if (int res_code = SendRpcReq(
            result_body, 1, method,
            DaemonRpc::GetArrayStr(std::vector{block_hex}), "POST /json_rpc");
        res_code != 200)
    {
        LOG_CODE_ERR(method, res_code, result_body);
        return false;
    }

    ondemand::object res;
    try
    {
        auto doc = parser.iterate(result_body.data(), result_body.size(),
                                  result_body.capacity());

        auto error = doc["result"].get_object().get(res);
        if (error != simdjson::error_code::SUCCESS)
        {
            res = doc["error"].get_object();
            int64_t err_code = res["code"].get_int64();
            std::string_view err_msg = res["message"].get_string();
            logger.Log<LogType::Error>(
                "Block submission rejected: error code: {}, message: {}",
                err_code, err_msg);
            return false;
        }
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}

bool DaemonManagerT<Coin::ZANO>::Transfer(TransferResCn& transfer_res,
                                 const std::vector<Payee>& dests, int64_t fee,
                                 simdjson::ondemand::parser& parser)
{
    std::string result_body;

    const auto method = "transfer"sv;

    std::vector<DaemonRpc::JsonStr> dest_strs;
    dest_strs.reserve(dests.size());

    for (const auto& [_id, amount, address] : dests)
    {
        dest_strs.push_back(
            DaemonRpc::ToJsonObj(std::make_pair("address"sv, address),
                                 std::make_pair("amount"sv, amount)));
    }

    std::string params = DaemonRpc::ToJsonObj(
        std::make_pair("destinations"sv, DaemonRpc::GetArrayStr(dest_strs)),
        std::make_pair("fee"sv, fee), std::make_pair("mixin"sv, 0));

    if (int res_code = SendRpcReq(result_body, 1, "transfer"sv, params,
                                  "POST /json_rpc"sv);
        res_code != 200)
    {
        LOG_CODE_ERR(method, res_code, result_body);
        return false;
    }

    try
    {
        transfer_res.doc = parser.iterate(
            result_body.data(), result_body.size(), result_body.capacity());

        auto obj = transfer_res.doc["result"].get_object();
        transfer_res.txid = obj["tx_hash"].get_string();
        transfer_res.tx_size = obj["tx_size"].get_int64();
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}

// zano daemon can shows orphaned blocks as confirmed...
bool DaemonManagerT<Coin::ZANO>::GetBlockHeaderByHash(BlockHeaderResCn& res,
                                             std::string_view block_hash,
                                             simdjson::ondemand::parser& parser)
{
    std::string result_body;

    const std::string_view method = "getblockheaderbyhash"sv;

    if (int res_code = SendRpcReq(
            result_body, 1, method,
            DaemonRpc::ToJsonObj(std::make_pair("hash"sv, block_hash)),
            "GET /json_rpc");
        res_code != 200)
    {
        LOG_CODE_ERR(method, res_code, result_body);
        return false;
    }

    try
    {
        res.doc = parser.iterate(result_body.data(), result_body.size(),
                                 result_body.capacity());

        auto obj = res.doc["result"].get_object();
        res.depth =
            static_cast<uint32_t>(obj["block_header"]["depth"].get_int64());
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}

bool DaemonManagerT<Coin::ZANO>::GetBlockHeaderByHeight(
    BlockHeaderResCn& res, uint32_t height, simdjson::ondemand::parser& parser)
{
    std::string result_body;

    const std::string_view method = "getblockheaderbyheight"sv;

    if (int res_code =
            SendRpcReq(result_body, 1, method,
                       DaemonRpc::ToJsonObj(std::make_pair("height"sv, height)),
                       "GET /json_rpc");
        res_code != 200)
    {
        LOG_CODE_ERR(method, res_code, result_body);
        return false;
    }

    try
    {
        res.doc = parser.iterate(result_body.data(), result_body.size(),
                                 result_body.capacity());

        auto obj = res.doc["result"].get_object();
        res.depth =
            static_cast<uint32_t>(obj["block_header"]["depth"].get_int64());
        res.hash = obj["block_header"]["hash"].get_string();
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}

bool DaemonManagerT<Coin::ZANO>::GetAliasAddress(AliasRes& res,
                                        std::string_view alias,
                                        simdjson::ondemand::parser& parser)
{
    std::string result_body;

    const std::string_view method = "get_alias_details"sv;

    if (int res_code = SendRpcReq(result_body, 1, method,
                                  DaemonRpc::ToJsonObj(std::make_pair("alias"sv, alias)), "GET /json_rpc");
        res_code != 200)
    {
        LOG_CODE_ERR(method, res_code, result_body);
        return false;
        }

    try
    {
        res.doc = parser.iterate(result_body.data(), result_body.size(),
                                  result_body.capacity());

        auto obj = res.doc["result"].get_object();
        res.address = obj["alias_details"]["address"].get_string();
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}


