#include "server.hpp"

template class Server<StratumClient>;

template <class T>
Server<T>::Server(int port, int timeout_sec)
    : timeout_sec(timeout_sec),
      tspec({.it_interval = timespec{.tv_sec = timeout_sec, .tv_nsec = 0},
             .it_value = timespec{.tv_sec = timeout_sec, .tv_nsec = 0}})
{
    epoll_fd = epoll_create1(0);
    timers_epoll_fd = epoll_create1(0);

    if (epoll_fd == -1)
    {
        throw std::invalid_argument(fmt::format(
            "Failed to create epoll: {} -> {}.", errno, std::strerror(errno)));
    }

    InitListeningSock(port);

    if (listen(listening_fd, MAX_CONNECTIONS_QUEUE) == -1)
        throw std::invalid_argument(
            "Stratum server failed to enter listenning state.");

    logger.Log<LogType::Info>("Created listening socket on port: {}", port);
}

template <class T>
Server<T>::~Server()
{
    for (auto &it : connections)
    {
        close(it->sockfd);
    }
    close(listening_fd);
    close(epoll_fd);

    logger.Log<LogType::Info>("Server destroyed. Connections closed.");
}

template <class T>
void Server<T>::Service()
{
    struct epoll_event events[MAX_CONNECTION_EVENTS];
    int epoll_res =
        epoll_wait(epoll_fd, events, MAX_CONNECTION_EVENTS,
                   EPOLL_TIMEOUT);

    for (int i = 0; i < epoll_res; i++)
    {
        auto event = events[i];
        uint32_t flags = event.events;

        if (event.data.fd != listening_fd)
        {
            auto *conn_it = reinterpret_cast<connection_it *>(event.data.ptr);

            if (!HandleEvent(conn_it, flags))
            {
                EraseClient(conn_it);
            }
            else
            {
                (*(*conn_it))->expiration_count = 0;
                timerfd_settime((*(*conn_it))->timerfd, 0, &tspec, nullptr);
            }
        }
        else
        {
            HandleNewConnection();
        }
    }

    if (epoll_res == -1)
    {
        if (errno == EBADF || errno == EFAULT || errno == EINVAL)
        {
            throw std::invalid_argument(fmt::format(
                "Failed to epoll_wait: {} -> {}", errno, std::strerror(errno)));
        }
        else if (errno != EINTR)
        {
            // EINTR, ignore
            logger.Log<LogType::Error>("Failed to epoll_wait: {} -> {}", errno,
                                       std::strerror(errno));
            return;
        }
    }

    // immediate check
    epoll_res = epoll_wait(timers_epoll_fd, events,
                           MAX_CONNECTION_EVENTS, 0);
    for (int i = 0; i < epoll_res; i++)
    {
        auto event = events[i];
        auto *conn_it = reinterpret_cast<connection_it *>(event.data.ptr);

        uint64_t expiration_count = 0;
        auto conn_ptr = (*(*conn_it));

        if (read(conn_ptr->timerfd, &expiration_count,
                 sizeof(expiration_count)) == -1 ||
            !HandleTimeout(conn_it,
                           conn_ptr->expiration_count + expiration_count) ||
            !RearmFd(conn_it, conn_ptr->timerfd, timers_epoll_fd))
        {
            EraseClient(conn_it);
        }
        conn_ptr->expiration_count += expiration_count;
    }
}

template <class T>
bool Server<T>::HandleEvent(connection_it *conn_it, uint32_t flags)
{
    int sockfd = (*conn_it)->get()->sockfd;

    if (flags & EPOLLERR)
    {
        int error = 0;

        if (socklen_t errlen = sizeof(error);
            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen) ==
            0)
        {
            logger.Log<LogType::Warn>(
                "Received epoll error on socket fd {}, "
                "errno: {} -> {}",
                sockfd, error, std::strerror(error));
        }

        return false;
    }

    return HandleReadable(conn_it);
}

template <class T>
bool Server<T>::HandleReadable(connection_it *it)
{
    std::shared_ptr<Connection<T>> conn = *(*it);
    const int sockfd = conn->sockfd;
    std::string ip(conn->ip);
    ssize_t recv_res = 0;

    while (true)
    {
        recv_res =
            recv(sockfd, conn->req_buff + conn->req_pos,
                 REQ_BUFF_SIZE_REAL - conn->req_pos - 1, 0);

        if (recv_res == -1)
        {
            if ((/* errno == EWOULDBLOCK || */ errno != EAGAIN) ||
                !RearmFd(it, conn->sockfd, epoll_fd))
            {
                logger.Log<LogType::Warn>(
                    "Client with ip {} disconnected because of socket (fd:"
                    "{}) error: {} -> {}.",
                    ip, sockfd, errno, std::strerror(errno));
                return false;
            }
            // EAGAIN allowed
            return true;
        }

        conn->req_pos += recv_res;
        conn->req_buff[conn->req_pos] = '\0';  // for strchr

        HandleConsumeable(it);

        // only erase the client after we had consumed all he had pending
        if (recv_res == 0)
        {
            // should happened on flooded buffer

            logger.Log<LogType::Info>(
                "Client with ip {} (sockfd {}) disconnected.", ip, sockfd);
            return false;
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
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    {
        std::unique_lock lock(connections_mutex);
        connections.emplace_back(std::make_shared<Connection<T>>(
            conn_fd, conn_addr.sin_addr, timerfd));
        connections.back()->it = --connections.end();

        conn_it = &connections.back()->it;
    }
    std::string ip((*(*conn_it))->ip);

    // since this is a union only one member can be assigned, data will be
    // assigned in rearm
    struct epoll_event empty_conn_ev
    {
        .events = 0, .data = {
            .ptr = nullptr,
        }
    };

    // only add to the interest list after all the connection data has been
    // created to avoid data races
    if (!HandleConnected(conn_it))
    {
        EraseClient(conn_it);
        return;
    }

    /* relative timer*/
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &empty_conn_ev) == -1 ||
        timerfd_settime(timerfd, 0, &tspec, nullptr) == -1 ||
        epoll_ctl(timers_epoll_fd, EPOLL_CTL_ADD, timerfd, &empty_conn_ev) ==
            -1 ||
        !RearmFd(conn_it, conn_fd, epoll_fd) ||
        !RearmFd(conn_it, timerfd, timers_epoll_fd))
    {
        EraseClient(conn_it);

        logger.Log<LogType::Warn>(
            "Failed to add socket / socket timer of client with ip {} to "
            "epoll list errno: {} "
            "-> errno: {}. ",
            ip, conn_fd, errno);
        return;
    }

    logger.Log<LogType::Info>("Tcp client connected, ip: {}, sockfd {}", ip,
                              conn_fd);
}

// ptr should point to struct that has Connection as its first member
template <class T>
bool Server<T>::RearmFd(connection_it *it, int fd, int efd) const
{
    Connection<T> *conn = (*it)->get();

    // rearm socket in epoll interest list ~1us
    if (epoll_event conn_ev{.events = EPOLLIN | EPOLLET | EPOLLONESHOT,
                            .data = {.ptr = it}};
        epoll_ctl(efd, EPOLL_CTL_MOD, fd, &conn_ev) == -1)
    {
        logger.Log<LogType::Warn>("Failed to rearm (fd: {}) to epoll (fd: {})",
                                  fd, efd);
        return false;
    }

    return true;
}

template <class T>
int Server<T>::AcceptConnection(sockaddr_in *addr, socklen_t *addr_size) const
{
    int flags = SOCK_NONBLOCK;
    int conn_fd = accept4(listening_fd, (sockaddr *)addr, addr_size, flags);

    if (conn_fd == -1)
    {
        return -1;
    }

    if (int yes = 1;
        setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1)
    {
        return -2;
    }

    return conn_fd;
}

template <class T>
void Server<T>::EraseClient(connection_it *it)
{
    const int sockfd = (*(*it))->sockfd;
    const int timerfd = (*(*it))->timerfd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, nullptr) == -1 ||
        epoll_ctl(timers_epoll_fd, EPOLL_CTL_DEL, timerfd, nullptr) == -1)
    {
        logger.Log<LogType::Warn>(
            "Failed to remove socket {} from epoll list errno: {} "
            "-> errno: {}. ",
            sockfd, errno, std::strerror(errno));
    }

    if (close(sockfd) == -1 || close(timerfd) == -1)
    {
        logger.Log<LogType::Warn>(
            "Failed to close socket {} errno: {} "
            "-> errno: {}. ",
            sockfd, errno, std::strerror(errno));
    }

    HandleDisconnected(it);

    // O(1)
    std::unique_lock lock(connections_mutex);
    connections.erase(*it);
}

template <class T>
void Server<T>::InitListeningSock(int port)
{
    int optval = 1;

    listening_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listening_fd == -1)
        throw std::invalid_argument("Failed to create stratum socket");

    if (setsockopt(listening_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) == -1)
        throw std::invalid_argument("Failed to set stratum socket options");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listening_fd, (const sockaddr *)&addr, sizeof(addr)) == -1)
    {
        throw std::invalid_argument(
            fmt::format("Stratum server failed to bind to port: {}", port));
    }

    struct epoll_event listener_ev;
    memset(&listener_ev, 0, sizeof(listener_ev));
    listener_ev.events = EPOLLIN | EPOLLET;
    listener_ev.data.fd = listening_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listening_fd, &listener_ev) == -1)
    {
        throw std::invalid_argument(
            fmt::format("Failed to add listener socket to epoll set: {} -> {}",
                        errno, std::strerror(errno)));
    }
}