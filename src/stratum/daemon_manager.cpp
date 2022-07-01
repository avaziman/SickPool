// #include "daemon_manager.hpp"

// template <typename... T>
// int DaemonManager::SendRpcReq(std::string& result, int id, const char* method,
//                               T... params)
// {
//     std::lock_guard rpc_lock(this->rpc_mutex);
//     for (DaemonRpc& rpc : rpcs)
//     {
//         int res = rpc.SendRequest(result, id, method, params...);
//         if (res != -1) return res;
//     }

//     return -2;
// }