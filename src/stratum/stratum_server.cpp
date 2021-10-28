#include "stratum_server.hpp"

StratumServer::StratumServer(unsigned long ip, unsigned short port)
{
    sockfd =
        socket(AF_INET,      // adress family: ipv4
               SOCK_STREAM,  // socket type: socket stream, reliable byte
                             // connection-based byte stream
               IPPROTO_TCP   // protocol: transmission control protocol (tcp)
        );

    if (sockfd == INVALID_SOCKET) throw -1;

    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = ip;
    addr.sin_port = port;

    if (bind(sockfd, (const sockaddr *)&addr, sizeof(addr)) != 0) throw -1;
}

void StratumServer::StartListening()
{
    if (listen(sockfd, 64) != 0)
        throw std::runtime_error(
            "Stratum server failed to enter listenning state.");
    std::thread(Listen, this).detach();
}

void StratumServer::Listen()
{
    while (true)
    {
        int addr_len;
        SOCKET conn_fd;
        sockaddr_in conn_addr;
        conn_fd = accept(sockfd, (sockaddr *)&addr, &addr_len);

        if (conn_fd <= 0)
        {
            std::cerr << "Invalid connecting socket accepted." << std::endl;
            continue;
        };

        std::thread(HandleSocket, this, conn_fd).detach();
    }
}

void StratumServer::HandleSocket(SOCKET conn_fd)
{
    while (true)
    {
        char buffer[1024];
        int res = recv(conn_fd, buffer, sizeof(buffer), 0);

        if (res <= 0)
        {
            std::cerr << "Failed to receive data from socket." << std::endl;
            continue;
        }

        HandleReq(buffer);
    }
}

void StratumServer::HandleReq(char buffer[])
{
    Document req(kObjectType);
    req.Parse(buffer);

    const char *method = req["method"].GetString();

    if (!strcmp(method, "mining.subscribe"))
        HandleSubscribe(&req);
    else if (!strcmp(method, "mining.authorize"))
        HandleAuthorize(&req);
}

void StratumServer::HandleSubscribe(Document *req)
{
    // Mining software info format: "SickMiner/6.9"
    const char *m_software_info = (*req)["params"][0].GetString();
    const char *last_session_id = (*req)["params"][1].GetString();
    const char *host = (*req)["params"][2].GetString();
    int port = (*req)["params"][3].GetInt();
}

void StratumServer::HandleAuthorize(Document *req)
{
    const char *name = (*req)["params"][0].GetString();
    const char *pass = (*req)["params"][1].GetString();
}