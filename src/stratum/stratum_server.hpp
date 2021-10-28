#ifndef STRATUM_SERVER_HPP_
#define STRATUM_SERVER_HPP_

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>  // inet_ntop, inet_pton
#else
#include <sys/socket.h>
#endif

#include <rapidjson/document.h>

#include <iostream>
#include <thread>

using namespace rapidjson;

class StratumServer
{
   public:
    StratumServer(unsigned long ip, unsigned short port);
    void StartListening();

   private:
    void Listen();
    void HandleSocket(SOCKET sockfd);
    void HandleReq(char buffer[]);
    void HandleSubscribe(Document* req);
    void HandleAuthorize(Document* req);

    SOCKET sockfd;
    sockaddr_in addr;
};
#endif