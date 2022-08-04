#ifndef DAEMON_MANAGER_HPP
#define DAEMON_MANAGER_HPP
#include <simdjson/simdjson.h>

#include <mutex>
#include <tuple>
#include <type_traits>

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
    bool is_mine;
};

struct GetIdentityRes
{
    simdjson::ondemand::document doc;

    std::string_view name;
};

class DaemonManager
{
   public:
    DaemonManager(const std::vector<RpcConfig>& rpc_configs)
    {
        for (const auto& config : rpc_configs)
        {
            rpcs.emplace_back(config.host, config.auth);
        }
    }

    bool GetIdentity(GetIdentityRes& id_res, simdjson::ondemand::parser& parser,
                     std::string_view addr)
    {
        using namespace simdjson;

        std::string result_body;
        int res_code = SendRpcReq<std::any>(result_body, 1, "getidentity",
                                            std::any(addr));

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
            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Failed to getidentity: {}",
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
        int res_code = SendRpcReq<std::any>(result_body, 1, "validateaddress",
                                            std::any(addr));

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
            va_res.is_mine = res["ismine"].get_bool();
        }
        catch (const simdjson_error& err)
        {
            Logger::Log(LogType::Critical, LogField::Stratum,
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
        int res_code =
            SendRpcReq<std::any>(res_body, 1, "getblock", std::any(block));

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
                        "Failed to parse getblock, error: {}", err.what());
            return false;
        }
        return true;
    }

    template <typename... T>
    int SendRpcReq(std::string& result, int id, const char* method, T... params)
    {
        std::lock_guard rpc_lock(this->rpc_mutex);
        for (DaemonRpc& rpc : rpcs)
        {
            int res = rpc.SendRequest(result, id, method, params...);
            if (res != -1) return res;
        }

        return -2;
    }

   private:
    std::vector<DaemonRpc> rpcs;
    std::mutex rpc_mutex;
};

#endif