#ifndef DAEMON_MANAGER_HPP
#define DAEMON_MANAGER_HPP
#include <mutex>
#include <tuple>
#include <type_traits>
#include "daemon/daemon_rpc.hpp"
#include "coin_config.hpp"

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
    // TODO: make this like printf for readability
    template <typename... T>
    int SendRpcReq(std::string& result, int id, const char* method,
                   T... params)
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