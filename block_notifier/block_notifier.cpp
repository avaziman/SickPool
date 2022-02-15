#ifdef _WIN32
#include <winsock2.h>
#else 
#include <sys/socket.h>
#include <unistd.h>  //close
#include <netinet/in.h>
#endif

#include <cstdio>
#include <iostream>

int main(int argc, char* argv[])
{
    const char* block_hash = argv[1];

#ifdef _WIN32
    WSADATA wsdata;
    if (WSAStartup(MAKEWORD(2, 2), &wsdata) != 0) exit(-1);
#endif

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in addr;

    addr.sin_family = AF_INET;
    #ifdef _WIN32
    addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    #else
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#endif
    addr.sin_port = htons(4444);

    if (connect(sockfd, (const sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "Failed to connect stratum server.";
        return -1;
    }

    char buffer[1024];
    int len = sprintf(buffer, "{\"id\": null, \"method\":\"mining.update_block\",\"params\":[\"%s\"]}\n",
            block_hash);

    send(sockfd, buffer, len, 0);
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
    #else
    close(sockfd);
    #endif
}