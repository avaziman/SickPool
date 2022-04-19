#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  //close

#include <cstdio>

#define PORT 4444

// stratum.block_notify
// stratum.wallet_notify
int main(int argc, char* argv[])
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);

    char* method = argv[1];
    char* param = argv[2];

    char message[128];
    // no need id
    int len = snprintf(message, sizeof(message),
                       "{\"id\":0,\"method\":\"stratum.%s\","
                       "\"params\":[\"%s\"]}\n",
                       method, param);

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));

    int res = send(sockfd, message, len, 0);
    close(sockfd);

    if (res == -1)
    {
        printf("Failed to send notify %s: %d\n", method, errno);
        return -1;
    }

    printf("Notified stratum server on %s: %s\n", method, param);

    return 0;
}