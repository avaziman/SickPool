#ifndef DAEMON_API_HPP_
#define DAEMON_API_HPP_

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>  // inet_ntop, inet_pton
#else
#include <sys/socket.h>
#endif

using namespace rapidjson;

class DaemonRpc
{
   public:
    DaemonRpc(u_long rpc_ip, u_short rpc_port, const char* auth_header);
    void SendRequest(int id, const char* method,
                     std::vector<const char*> params);
    ~DaemonRpc();

   private:
    void Init();
    SOCKET sockfd;
    sockaddr_in rpc_addr;
    const char* auth_header;
    std::string host_header;
};
#endif