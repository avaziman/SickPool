#include "daemon_manager_vrsc.hpp"

bool DaemonManagerT<Coin::VRSC>::GetBlock(BlockRes& block_res,
                                          simdjson::ondemand::parser& parser,
                                          std::string_view block)
{
    using namespace simdjson;

    constexpr std::string_view method = "getblock";

    std::string res_body;

    if (int res_code = SendRpcReq(res_body, 1, method,
                                  DaemonRpc::GetArrayStr(std::vector{block}));
        res_code != 200)
    {
        return false;
    }

    try
    {
        block_res.doc = parser.iterate(res_body.data(), res_body.size(),
                                       res_body.capacity());

        ondemand::object res = block_res.doc["result"].get_object();
        block_res.validation_type = res["validationtype"] == "stake"
                                        ? ValidationType::STAKE
                                        : ValidationType::WORK;
        block_res.confirmations =
            static_cast<int>(res["confirmations"].get_int64());
        block_res.height = static_cast<uint32_t>(res["height"].get_int64());

        for (auto tx_id : res["tx"].get_array())
        {
            block_res.tx_ids.emplace_back(tx_id.get_string());
        }
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);

        return false;
    }
    return true;
}

bool DaemonManagerT<Coin::VRSC>::ValidateAddress(
    ValidateAddressRes& va_res, simdjson::ondemand::parser& parser,
    std::string_view addr)
{
    using namespace simdjson;

    constexpr std::string_view method = "validateaddress";
    std::string result_body;

    if (int res_code = SendRpcReq(result_body, 1, method,
                                  DaemonRpc::GetArrayStr(std::vector{addr}));
        res_code != 200)
    {
        return false;
    }

    ondemand::object res;
    try
    {
        va_res.doc = parser.iterate(result_body.data(), result_body.size(),
                                    result_body.capacity());

        res = va_res.doc["result"].get_object();

        va_res.is_valid = res["isvalid"].get_bool();

        va_res.valid_addr = res["address"].get_string();
        va_res.script_pub_key = res["scriptPubKey"].get_string();
        // va_res.is_mine = res["ismine"].get_bool();
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}

bool DaemonManagerT<Coin::VRSC>::FundRawTransaction(
    FundRawTransactionRes& fund_res, simdjson::ondemand::parser& parser,
    std::string_view raw_tx, int fee_rate, std::string_view change_addr)
{
    using namespace simdjson;

    constexpr std::string_view method = "fundrawtransaction";
    std::string result_body;
    std::string params =
        fmt::format("[\"{}\",{{\"fee_rate\":{},\"changeAddress\":\"{}\"}}]",
                    raw_tx, fee_rate, change_addr);

    if (int res_code = SendRpcReq(result_body, 1, method, params);
        res_code != 200)
    {
        logger.Log<LogType::Error>("Failed to fundrawtransaction, response: {}",
                                   result_body);
        return false;
    }

    ondemand::object res;
    try
    {
        fund_res.doc = parser.iterate(result_body.data(), result_body.size(),
                                      result_body.capacity());

        res = fund_res.doc["result"].get_object();

        fund_res.hex = res["hex"].get_string();
        fund_res.fee = res["fee"].get_double();
        fund_res.changepos = static_cast<int>(res["changepos"].get_int64());
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}

bool DaemonManagerT<Coin::VRSC>::SignRawTransaction(
    SignRawTransactionRes& sign_res, simdjson::ondemand::parser& parser,
    std::string_view funded_tx)
{
    using namespace simdjson;
    using namespace std::string_view_literals;

    std::string result_body;
    constexpr std::string_view method = "signrawtransactionwithwallet";

    if (int res_code =
            SendRpcReq(result_body, 1, method,
                       DaemonRpc::GetArrayStr(std::vector{funded_tx}));
        res_code != 200)
    {
        logger.Log<LogType::Error>(
            "Failed to signrawtransaction, response: {}, rawtx: {}",
            result_body, funded_tx);
        return false;
    }

    ondemand::object res;
    try
    {
        sign_res.doc = parser.iterate(result_body.data(), result_body.size(),
                                      result_body.capacity());

        res = sign_res.doc["result"].get_object();

        sign_res.hex = res["hex"].get_string();
        sign_res.complete = res["complete"].get_bool();
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}

bool DaemonManagerT<Coin::VRSC>::SubmitBlock(const std::string_view block_hex,
                                             simdjson::ondemand::parser& parser)
{
    std::string resultBody;
    constexpr std::string_view method = "submitblock";

    if (int resCode =
            SendRpcReq(resultBody, 1, method,
                       DaemonRpc::GetArrayStr(std::vector{block_hex}));
        resCode != 200)
    {
        logger.Log<LogType::Error>(
            "Failed to send block submission, http code: {}, res: {}", resCode,
            resultBody);
        return false;
    }

    try
    {
        using namespace simdjson;
        ondemand::document doc = parser.iterate(
            resultBody.data(), resultBody.size(), resultBody.capacity());

        ondemand::object res = doc.get_object();
        ondemand::value resultField = res["result"];

        if (ondemand::value errorField = res["error"]; !errorField.is_null())
        {
            std::string_view err_res = errorField.get_string();
            logger.Log<LogType::Error>(
                "Block submission rejected, rpc error: {}", err_res);
            return false;
        }

        if (!resultField.is_null())
        {
            std::string_view result = resultField.get_string();
            logger.Log<LogType::Error>(
                "Block submission rejected, rpc result: {}", result);

            if (result == "inconclusive")
            {
                logger.Log<LogType::Error>(
                    "Submitted inconclusive block, waiting for result...");
                return true;
            }
            return false;
        }
    }
    catch (const simdjson::simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }

    return true;
}

bool DaemonManagerT<Coin::VRSC>::GetBlockTemplate(
    BlockTemplateRes& templateRes, simdjson::ondemand::parser& parser)
{
    using namespace simdjson;

    std::string resultBody;
    constexpr std::string_view method = "getblocktemplate";

    if (int resCode = SendRpcReq(resultBody, 1, method,
                                 DaemonRpc::GetArrayStr(std::vector<int>{}));
        resCode != 200)
    {
        logger.Log<LogType::Error>(
            "Failed to send block submission, http code: {}, res: {}", resCode,
            resultBody);
        return false;
    }

    try
    {
        ondemand::document doc = parser.iterate(
            resultBody.data(), resultBody.size(), resultBody.capacity());

        ondemand::object res = doc["result"].get_object();

        templateRes.version = static_cast<uint32_t>(res["version"].get_int64());
        templateRes.prev_block_hash = res["previousblockhash"].get_string();
        templateRes.final_sroot_hash = res["finalsaplingroothash"].get_string();
        templateRes.solution = res["solution"].get_string();
        // can't iterate after we get the string_view
        ondemand::array txs = res["transactions"].get_array();

        // COINBASE TX
        templateRes.transactions.push_back(TxRes{});
        for (auto tx : txs)
        {
            std::string_view tx_data_hex = tx["data"].get_string();
            std::string_view tx_hash_hex = tx["hash"].get_string();
            double fee = tx["fee"].get_double();

            templateRes.transactions.emplace_back(tx_data_hex, tx_hash_hex,
                                                  fee);
        }

        TxRes coinbase{.data = res["coinbasetxn"]["data"].get_string(),
                       .hash = res["coinbasetxn"]["hash"].get_string(),
                       .fee = res["coinbasetxn"]["fee"].get_int64()};

        templateRes.transactions[0] = coinbase;
        templateRes.coinbase_value =
            res["coinbasetxn"]["coinbasevalue"].get_int64();

        templateRes.target = res["target"].get_string();
        templateRes.min_time = res["mintime"].get_int64();
        std::string_view bits_sv = res["bits"].get_string();
        std::from_chars(bits_sv.data(), bits_sv.data() + bits_sv.size(),
                        templateRes.bits, 16);

        templateRes.height = static_cast<uint32_t>(res["height"].get_int64());
    }
    catch (const simdjson::simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);
        return false;
    }
    return true;
}

bool DaemonManagerT<Coin::VRSC>::GetAliasAddress(
    AliasRes& id_res, std::string_view addr, simdjson::ondemand::parser& parser)
{
    using namespace simdjson;

    constexpr std::string_view method = "getidentity";
    std::string result_body;

    if (int res_code = SendRpcReq(result_body, 1, method,
                                  DaemonRpc::GetArrayStr(std::vector{addr}));
        res_code != 200)
    {
        return false;
    }

    ondemand::object res;
    try
    {
        id_res.doc = parser.iterate(result_body.data(), result_body.size(),
                                    result_body.capacity());

        res = id_res.doc["result"].get_object();

        id_res.address = res["identity"]["identityaddress"].get_string();
    }
    catch (const simdjson_error& err)
    {
        LOG_PARSE_ERR(method, err);

        return false;
    }

    return true;
}
