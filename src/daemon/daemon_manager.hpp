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


struct ValidateAddressRes
{
    simdjson::ondemand::document doc;

    bool is_valid;
    std::string_view valid_addr;
    std::string_view script_pub_key;
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
    virtual ~DaemonManager() = default;

    // parser needs to be in scope when object is used
    
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
    const Logger logger{logger_field};

   private:
    std::vector<DaemonRpc> rpcs;
    std::mutex rpc_mutex;
};

#endif