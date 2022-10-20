#include "server.hpp"

template class Server<StratumClient>;

template <class T>
Server<T>::Server(int port)
{
    epoll_fd = epoll_create1(0);

    if (epoll_fd == -1)
    {
        throw std::runtime_error(fmt::format(
            "Failed to create epoll: {} -> {}.", errno, std::strerror(errno)));
    }

    InitListeningSock(port);

    if (listen(listening_fd, MAX_CONNECTIONS_QUEUE) == -1)
        throw std::runtime_error(
            "Stratum server failed to enter listenning state.");

    logger.Log<LogType::Info>(
                "Created listening socket on port: {}", port);
}

template <class T>
Server<T>::~Server()
{
    for (auto &it : connections)
    {
        close(it->sock);
    }
    close(listening_fd);
    close(epoll_fd);

    logger.Log<LogType::Info>(
                "Server destroyed. Connections closed.");
}

template <class T>
void Server<T>::Service()
{
    struct epoll_event events[MAX_CONNECTION_EVENTS];
    int epoll_res =
        epoll_wait(epoll_fd, events, MAX_CONNECTION_EVENTS, EPOLL_TIMEOUT);

    if (epoll_res == -1)
    {
        if (errno == EBADF || errno == EFAULT || errno == EINVAL)
        {
            throw std::runtime_error(fmt::format(
                "Failed to epoll_wait: {} -> {}", errno, std::strerror(errno)));
        }
        else
        {
            // EINTR, ignore
            logger.Log<LogType::Error>(
                        "Failed to epoll_wait: {} -> {}", errno,
                        std::strerror(errno));
            return;
        }
    }

    for (int i = 0; i < epoll_res; i++)
    {
        auto event = events[i];
        auto flags = event.events;

        if (event.data.fd != listening_fd)
        {
            auto *conn_it = (connection_it *)(event.data.ptr);
            int sockfd = (*conn_it)->get()->sock;


            if (flags & EPOLLERR)
            {
                int error = 0;
                socklen_t errlen = sizeof(error);
                if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void *)&error,
                               &errlen) == 0)
                {
                    logger.Log<LogType::Warn>(
                                "Received epoll error on socket fd {}, "
                                "errno: {} -> {}",
                                sockfd, error, std::strerror(error));
                }

                EraseClient(conn_it);
                return;
            }

            HandleReadable(conn_it);
        }
        else
        {
            HandleNewConnection();
        }
    }
}

template <class T>
void Server<T>::HandleReadable(connection_it *it)
{
    std::shared_ptr<Connection<T>> conn = *(*it);
    const int sockfd = conn->sock;
    std::string ip(conn->ip);
    ssize_t recv_res = 0;

    while (true)
    {
        recv_res = recv(sockfd, conn->req_buff + conn->req_pos,
                        REQ_BUFF_SIZE_REAL - conn->req_pos - 1, 0);

        if (recv_res == -1)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                RearmSocket(it);
            }
            else
            {
                EraseClient(it);

                logger.Log<LogType::Warn>(
                            "Client with ip {} disconnected because of socket (fd:"
                            "{}) error: {} -> {}.",
                            ip, sockfd, errno, std::strerror(errno));
            }
            return;
        }

        conn->req_pos += recv_res;
        conn->req_buff[conn->req_pos] = '\0';  // for strchr

        HandleConsumeable(it);

        // only erase the client after we had consumed all he had pending
        if (recv_res == 0)
        {
            // should happened on flooded buffer
            EraseClient(it);

            logger.Log<LogType::Info>(
                        "Client with ip {} (sock {}) disconnected.", ip, sockfd);
            return;
        }
    }
}

template <class T>
void Server<T>::HandleNewConnection()
{
    struct sockaddr_in conn_addr;
    socklen_t addr_len = sizeof(conn_addr);

    int conn_fd = AcceptConnection(&conn_addr, &addr_len);

    if (conn_fd < 0)
    {
        logger.Log<LogType::Warn>(
                    "Failed to accept socket to errno: {} "
                    "-> errno: {}. ",
                    conn_fd, errno);
        return;
    }

    connection_it *conn_it = nullptr;

    {
        std::unique_lock lock(connections_mutex);
        connections.emplace_back(
            std::make_shared<Connection<T>>(conn_fd, conn_addr.sin_addr));
        connections.back()->it = --connections.end();

        conn_it = &connections.back()->it;
    }
    std::string ip((*(*conn_it))->ip);

    // since this is a union only one member can be assigned
    struct epoll_event conn_ev
    {
        .events = EPOLLIN | EPOLLET | EPOLLONESHOT, .data = {
            .ptr = conn_it,
        }
    };

    // only add to the interest list after all the connection data has been
    // created to avoid data races
    HandleConnected(conn_it);

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &conn_ev) == -1)
    {
        EraseClient(conn_it);

        logger.Log<
            LogType::Warn>(
            "Failed to add socket of client with ip {} to epoll list errno: {} "
            "-> errno: {}. ",
            ip, conn_fd, errno);
        return;
    }

    logger.Log<LogType::Info>(
                "Tcp client connected, ip: {}, sock {}", ip, conn_fd);
}

// ptr should point to struct that has Connection as its first member
template <class T>
bool Server<T>::RearmSocket(connection_it *it)
{
    Connection<T> *conn = (*it)->get();
    const int sockfd = conn->sock;

    struct epoll_event conn_ev
    {
        .events = EPOLLIN | EPOLLET | EPOLLONESHOT, .data = {.ptr = it }
    };

    // rearm socket in epoll interest list ~1us
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &conn_ev) == -1)
    {
        std::string ip(conn->ip);

        EraseClient(it);

        logger.Log<LogType::Warn>(
                    "Failed to rearm socket (fd: {}) of client with ip {} to"
                    "epoll list errno: {} "
                    "-> errno: {}. ",
                    sockfd, ip, errno, std::strerror(errno));
        return false;
    }

    return true;
}

template <class T>
int Server<T>::AcceptConnection(sockaddr_in *addr, socklen_t *addr_size)
{
    int flags = SOCK_NONBLOCK;
    int conn_fd = accept4(listening_fd, (sockaddr *)addr, addr_size, flags);

    if (conn_fd == -1)
    {
        return -1;
    }

    int yes = 1;
    if (setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1)
    {
        return -2;
    }
    return conn_fd;
}

template <class T>
void Server<T>::EraseClient(connection_it *it)
{
    const int sockfd = (*it)->get()->sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, nullptr) == -1)
    {
        logger.Log<LogType::Warn>(
                    "Failed to remove socket {} from epoll list errno: {} "
                    "-> errno: {}. ",
                    sockfd, errno, std::strerror(errno));
    }
    if (close(sockfd) == -1)
    {
        logger.Log<LogType::Warn>(
                    "Failed to close socket {} errno: {} "
                    "-> errno: {}. ",
                    sockfd, errno, std::strerror(errno));
    }

    HandleDisconnected(it);

    std::unique_lock lock(connections_mutex);
    connections.erase(*it);
}

template <class T>
void Server<T>::InitListeningSock(int port)
{
    int optval = 1;

    listening_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listening_fd == -1)
        throw std::runtime_error("Failed to create stratum socket");

    if (setsockopt(listening_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) == -1)
        throw std::runtime_error("Failed to set stratum socket options");

    // struct timeval timeout;
    // timeout.tv_sec = coin_config.socket_recv_timeout_seconds;
    // timeout.tv_usec = 0;
    // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
    // sizeof(timeout)) == -1)
    //     throw std::runtime_error("Failed to set stratum socket options");
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listening_fd, (const sockaddr *)&addr, sizeof(addr)) == -1)
    {
        throw std::runtime_error(
            fmt::format("Stratum server failed to bind to port: {}", port));
    }

    struct epoll_event listener_ev;
    memset(&listener_ev, 0, sizeof(listener_ev));
    listener_ev.events = EPOLLIN | EPOLLET;
    listener_ev.data.fd = listening_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listening_fd, &listener_ev) == -1)
    {
        throw std::runtime_error(
            fmt::format("Failed to add listener socket to epoll set: {} -> {}",
                        errno, std::strerror(errno)));
    }
}