#ifndef DAEMON_API_HPP_
#define DAEMON_API_HPP_

#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>  // inet_ntop, inet_pton
#else
#include <arpa/inet.h>  //inet_ntop
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  //close
#endif

#include <simdjson.h>

#define HTTP_HEADER_SIZE (1024 * 4)

class DaemonRpc
{
   public:
    DaemonRpc(std::string host_header, std::string auth_header);
    int SendRequest(std::vector<char>& result, int id, const char* method,
                    const char* params, int len);

   private:
    int sockfd;
    sockaddr_in rpc_addr;
    std::string auth_header;
    std::string host_header;
};
#endif