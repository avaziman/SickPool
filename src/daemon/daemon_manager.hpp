#ifndef DAEMON_MANAGER_HPP
#define DAEMON_MANAGER_HPP
#include <fmt/format.h>
#include <simdjson/simdjson.h>

#include <mutex>
#include <tuple>
#include <type_traits>

#include "daemon_rpc.hpp"
#include "logger.hpp"

#define LOG_CODE_ERR(method, code, res) logger.Log<LogType::Error>("Failed to {}, rescode: {}, response: {}", method, code, res)
#define LOG_PARSE_ERR(method, err) logger.Log<LogType::Error>("Failed to parse {} response: {}", method, err.what())

enum class ValidationType
{
    WORK,
    STAKE
};

struct BlockRes
{
    simdjson::ondemand::document doc;

    ValidationType validation_type;
    int confirmations;
    uint32_t height;
    std::vector<std::string_view> tx_ids;
    std::string err;
};

struct ValidateAddressRes
{
    simdjson::ondemand::document doc;

    bool is_valid;
    std::string_view valid_addr;
    std::string_view script_pub_key;
    std::string err;
};

struct GetIdentityRes
{
    simdjson::ondemand::document doc;

    std::string_view name;
    std::string_view err;
};

struct FundRawTransactionRes
{
    simdjson::ondemand::document doc;

    std::string_view hex;
    int changepos;
    double fee;
    std::string err;
};

struct SignRawTransactionRes
{
    simdjson::ondemand::document doc;

    std::string_view hex;
    bool complete;
    std::string err;
};

struct RpcConfig
{
    std::string host;
    std::string auth;
};

class DaemonManager
{
   public:
    explicit DaemonManager(const std::vector<RpcConfig>& rpc_configs)
    {
        for (const auto& config : rpc_configs)
        {
            rpcs.emplace_back(config.host, config.auth);
        }
    }

    bool SubmitBlock(const std::string_view block_hex,
                     simdjson::ondemand::parser& parser)
    {
        std::string resultBody;
        constexpr std::string_view method = "submitblock";
        int resCode = SendRpcReq(resultBody, 1, method,
                                 DaemonRpc::GetParamsStr(block_hex));

        if (resCode != 200)
        {
            logger.Log<LogType::Error>(
                "Failed to send block submission, http code: {}, res: {}",
                resCode, resultBody);
            return false;
        }

        try
        {
            using namespace simdjson;
            ondemand::document doc = parser.iterate(
                resultBody.data(), resultBody.size(), resultBody.capacity());

            ondemand::object res = doc.get_object();
            ondemand::value resultField = res["result"];

            if (ondemand::value errorField = res["error"];
                !errorField.is_null())
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

    double GetNetworkHashPs(simdjson::ondemand::parser& parser)
    {
        using namespace simdjson;
        using namespace std::string_view_literals;

        constexpr std::string_view method = "getnetworkhashps";
        std::string result_body;
        int res_code = SendRpcReq(result_body, 1, method);

        if (res_code != 200)
        {
            return false;
        }

        ondemand::object res;
        try
        {
            auto doc = parser.iterate(result_body.data(), result_body.size(),
                                      result_body.capacity());

            return doc["result"].get_double();
        }
        catch (const simdjson_error& err)
        {
            LOG_PARSE_ERR(method, err);
            return 0;
        }

        return 0;
    }

    bool SignRawTransaction(SignRawTransactionRes& sign_res,
                            simdjson::ondemand::parser& parser,
                            std::string_view funded_tx)
    {
        using namespace simdjson;
        using namespace std::string_view_literals;

        std::string result_body;
        constexpr std::string_view method = "signrawtransactionwithwallet";
        int res_code = SendRpcReq(result_body, 1, method,
                                  DaemonRpc::GetParamsStr(funded_tx));

        if (res_code != 200)
        {
            logger.Log<LogType::Error>(
                "Failed to signrawtransaction, response: {}, rawtx: {}",
                result_body, funded_tx);
            return false;
        }

        ondemand::object res;
        try
        {
            sign_res.doc = parser.iterate(
                result_body.data(), result_body.size(), result_body.capacity());

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

    bool FundRawTransaction(FundRawTransactionRes& fund_res,
                            simdjson::ondemand::parser& parser,
                            std::string_view raw_tx, int fee_rate,
                            std::string_view change_addr)
    {
        using namespace simdjson;

        constexpr std::string_view method = "fundrawtransaction";
        std::string result_body;
        std::string params =
            fmt::format("[\"{}\",{{\"fee_rate\":{},\"changeAddress\":\"{}\"}}]",
                        raw_tx, fee_rate, change_addr);
        int res_code = SendRpcReq(result_body, 1, method, params);

        if (res_code != 200)
        {
            logger.Log<LogType::Error>(
                "Failed to fundrawtransaction, response: {}", result_body);
            return false;
        }

        ondemand::object res;
        try
        {
            fund_res.doc = parser.iterate(
                result_body.data(), result_body.size(), result_body.capacity());

            res = fund_res.doc["result"].get_object();

            fund_res.hex = res["hex"].get_string();
            fund_res.fee = res["fee"].get_double();
            fund_res.changepos = res["changepos"].get_int64();
        }
        catch (const simdjson_error& err)
        {
            LOG_PARSE_ERR(method, err);
            return false;
        }

        return true;
    }

    bool GetIdentity(GetIdentityRes& id_res, simdjson::ondemand::parser& parser,
                     std::string_view addr)
    {
        using namespace simdjson;

        constexpr std::string_view method = "getidentity";
        std::string result_body;
        int res_code = SendRpcReq(result_body, 1, method,
                                  DaemonRpc::GetParamsStr(addr));
        if (res_code != 200)
        {
            return false;
        }

        ondemand::object res;
        try
        {
            id_res.doc = parser.iterate(result_body.data(), result_body.size(),
                                        result_body.capacity());

            res = id_res.doc["result"].get_object();

            id_res.name = res["identity"]["name"].get_string();
        }
        catch (const simdjson_error& err)
        {
            LOG_PARSE_ERR(method, err);

            return false;
        }

        return true;
    }

    bool ValidateAddress(ValidateAddressRes& va_res,
                         simdjson::ondemand::parser& parser,
                         std::string_view addr)
    {
        using namespace simdjson;

        constexpr std::string_view method = "validateaddress";
        std::string result_body;
        int res_code = SendRpcReq(result_body, 1, method,
                                  DaemonRpc::GetParamsStr(addr));

        if (res_code != 200)
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

    // block hash or number (both sent as string),
    // parser needs to be in scope when object is used
    bool GetBlock(BlockRes& block_res, simdjson::ondemand::parser& parser,
                  std::string_view block)
    {
        using namespace simdjson;

        constexpr std::string_view method = "getblock";

            std::string res_body;
        int res_code = SendRpcReq(res_body, 1, method,
                                  DaemonRpc::GetParamsStr(block));

        if (res_code != 200)
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
            block_res.confirmations = res["confirmations"].get_int64();
            block_res.height = res["height"].get_int64();

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

    int SendRpcReq(std::string& result, int id, std::string_view method,
                   std::string_view params = "[]",
                   std::string_view type = "POST /")
    {
        std::unique_lock rpc_lock(rpc_mutex);
        for (DaemonRpc& rpc : rpcs)
        {
            int res = rpc.SendRequest(result, id, method, params, type);
            if (res != -1) return res;
        }

        return -2;
    }

   protected:
    static constexpr std::string_view logger_field = "DaemonManager";
    const Logger<logger_field> logger;

   private:
    std::vector<DaemonRpc> rpcs;
    std::mutex rpc_mutex;
};

#endif