#ifndef DAEMON_MANAGER_HPP
#define DAEMON_MANAGER_HPP
#include <simdjson/simdjson.h>

#include <mutex>
#include <tuple>
#include <type_traits>

#include <fmt/format.h>
#include "../coin_config.hpp"
#include "../daemon/daemon_rpc.hpp"
#include "logger.hpp"

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
};

struct ValidateAddressRes
{
    simdjson::ondemand::document doc;

    bool is_valid;
    std::string_view valid_addr;
    std::string_view script_pub_key;
    // bool is_mine;
};

struct GetIdentityRes
{
    simdjson::ondemand::document doc;

    std::string_view name;
};

struct FundRawTransactionRes
{
    simdjson::ondemand::document doc;

    std::string_view hex;
    int changepos;
    double fee;
};

struct SignRawTransactionRes
{
    simdjson::ondemand::document doc;

    std::string_view hex;
    bool complete;
};
class DaemonManager
{
   public:
    DaemonManager(const std::vector<RpcConfig>& rpc_configs): rpc_mutex()
    {
        for (const auto& config : rpc_configs)
        {
            rpcs.emplace_back(config.host, config.auth);
        }
    }

    double GetNetworkHashPs(simdjson::ondemand::parser& parser)
    {
        using namespace simdjson;
        using namespace std::string_view_literals;

        std::string result_body;
        int res_code = SendRpcReq(result_body, 1, "getnetworkhashps");

        if (res_code != 200)
        {
            return false;
        }

        ondemand::object res;
        try
        {
            auto doc = parser.iterate(
                result_body.data(), result_body.size(), result_body.capacity());

            return doc["result"].get_double();
        }
        catch (const simdjson_error& err)
        {
            Logger::Log(LogType::Warn, LogField::DaemonManager,
                        "Failed to parse getnetworkhashps response: {}", err.what());
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
        int res_code = SendRpcReq(result_body, 1, "signrawtransactionwithwallet",
                                  DaemonRpc::GetParamsStr(funded_tx));

        if (res_code != 200)
        {
            Logger::Log(LogType::Warn, LogField::DaemonManager,
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
            Logger::Log(LogType::Warn, LogField::DaemonManager,
                        "Failed to parse signrawtransaction response: {}", err.what());
            return false;
        }

        return true;
    }

    bool FundRawTransaction(FundRawTransactionRes& fund_res,
                            simdjson::ondemand::parser& parser,
                            std::string_view raw_tx, int fee_rate, std::string_view change_addr)
    {
        using namespace simdjson;

        std::string result_body;
        std::string params =
            fmt::format("[\"{}\",{{\"fee_rate\":{},\"changeAddress\":\"{}\"}}]", raw_tx, fee_rate, change_addr);
        int res_code = SendRpcReq(result_body, 1, "fundrawtransaction", params);

        if (res_code != 200)
        {
            Logger::Log(LogType::Warn, LogField::DaemonManager,
                        "Failed to fundrawtransaction, response: {}", result_body);
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
            fund_res.changepos = res["changepos"].get_int64();
        }
        catch (const simdjson_error& err)
        {
            Logger::Log(LogType::Warn, LogField::DaemonManager,
                            "Failed to parse fundrawtransaction response: {}, body: {}", err.what(), result_body);
                    
            return false;
        }

        return true;
    }

    bool GetIdentity(GetIdentityRes& id_res, simdjson::ondemand::parser& parser,
                     std::string_view addr)
    {
        using namespace simdjson;


        std::string result_body;
        int res_code = SendRpcReq(result_body, 1, "getidentity",
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
            Logger::Log(LogType::Warn, LogField::DaemonManager,
                        "Failed to parse getidentity response: {}",
                        err.what());
            return false;
        }

        return true;
    }

    bool ValidateAddress(ValidateAddressRes& va_res,
                         simdjson::ondemand::parser& parser,
                         std::string_view addr)
    {
        using namespace simdjson;

        std::string result_body;
        int res_code =
            SendRpcReq(result_body, 1, "validateaddress",
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
            Logger::Log(LogType::Warn, LogField::DaemonManager,
                        "Authorize RPC (validateaddress) failed: {}",
                        err.what());
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

        std::string res_body;
        int res_code = SendRpcReq(
            res_body, 1, std::string_view("getblock"), DaemonRpc::GetParamsStr(block));

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
            Logger::Log(LogType::Error, LogField::Stratum,
                        "Failed to parse getblock response, error: {}",
                        err.what());
            return false;
        }
        return true;
    }

    int SendRpcReq(std::string& result, int id, std::string_view method, std::string_view params = "[]", std::string_view type = "POST /")
    {
        std::unique_lock rpc_lock(rpc_mutex);
        for (DaemonRpc& rpc : rpcs)
        {
            int res = rpc.SendRequest(result, id, method, params, type);
            if (res != -1) return res;
        }

        return -2;
    }

   private:
    std::vector<DaemonRpc> rpcs;
    std::mutex rpc_mutex;
};


#endif