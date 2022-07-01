#ifndef DAEMON_API_HPP_
#define DAEMON_API_HPP_

#include <sstream>
#include <string>
#include <vector>

#include <arpa/inet.h>  //inet_ntop
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  //close

#include <simdjson.h>

#define HTTP_HEADER_SIZE (1024 * 4)

class DaemonRpc
{
   public:
    DaemonRpc(const std::string& host_header, const std::string& auth_header);
    
    int SendRequest(std::string &result, int id,
                              const char *method, const char *params,
                              std::size_t paramsLen);

   private:
    int sockfd;
    sockaddr_in rpc_addr;
    std::string host_header;
    std::string auth_header;
};
#endif