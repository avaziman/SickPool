#include "stratum_notifier_lib.hpp"
#define PORT 1111

int stratum_notify(const char* method, const char* param)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);

    char message[128];
    // no need id
    int len = snprintf(message, sizeof(message), "%c %s", method[0], param);

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));

    // don't even care about connect res, just send and ignore signals
    ssize_t res = send(sockfd, message, len, MSG_NOSIGNAL);
    close(sockfd);

    if (res == -1)
    {
        printf("Failed to send notify %s: %d\n", method, errno);
        return -1;
    }

    printf("Notified stratum server on %s: %s\n", method, param);
    return 0;
}