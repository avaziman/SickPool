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
#include <arpa/inet.h> //inet_ntop
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  //close
#endif

#define HEADER_SIZE 1024

using namespace rapidjson;

class DaemonRpc
{
   public:
    DaemonRpc(std::string host_header, std::string auth_header);
    char* SendRequest(int id, std::string method, std::string params = "");

   private:
    int sockfd;
    sockaddr_in rpc_addr;
    std::string auth_header;
    std::string host_header;
    
};
#endif