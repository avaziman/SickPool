#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  //close
#include <errno.h>
#include <cstdio>

#define PORT 4444

int main(int argc, char** argv) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);

    char message[128];
    // no need id
    int len = snprintf(message, sizeof(message),
             "{\"method\":\"mining.update_block\", \"params\":[\"%s\"]}\n",
             argv[1]);

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));

    int res = send(sockfd, message, len, 0);
    close(sockfd);

    if(res == -1){
        printf("Failed to send block update: %d\n", errno);
        return -1;
    }

    printf("Notified stratum server on block: %s\n", argv[1]);

    return 0;
}