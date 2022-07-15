#include <benchmark/benchmark.h>
#include <sys/socket.h>
#include <netinet/in.h>

static void BM_Submit(benchmark::State& state)
{
    char send_buff[1024 * 4];
    char recv_buff[1024 * 4];

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in pool_addr;
    pool_addr.sin_port = htons(4000);
    pool_addr.sin_addr.s_addr = INADDR_ANY;
    pool_addr.sin_family = AF_INET;
    connect(sock, (const sockaddr*)&pool_addr, sizeof(pool_addr));

    for (auto _ : state)
    {
        int len = snprintf(send_buff, sizeof(send_buff), "OK");
        send(sock, send_buff, len, 0);

        recv(sock, recv_buff, sizeof(recv_buff), 0);
    }
}

BENCHMARK(BM_Submit);