#ifndef SERVER_HPP_
#define SERVER_HPP_

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <list>
#include <memory>

#include "connection.hpp"
#include "logger.hpp"
#include "stratum_client.hpp"

// enum ConnectionEventType
// {
//     NONE,
//     CONNECTED,
//     DISCONNECTED,
//     READABLE
// };

// template <typename T>
// struct ConnectionEvent
// {
//     ConnectionEvent(ConnectionEventType t, T* p) : type(t), ptr(p){}
//     ConnectionEventType type;
//     T* ptr;
// };
inline constexpr std::string_view field_str = "Server";

template <typename T>
class Server
{
   public:
    using connection_it = std::list<std::shared_ptr<Connection<T>>>::iterator;

    explicit Server(int port, int timeout_sec);
    ~Server();
    void Service();

    virtual void HandleConsumeable(connection_it* conn) = 0;
    // returns whether to reject the connection;
    virtual bool HandleConnected(connection_it* conn) = 0;
    virtual bool HandleTimeout(connection_it* conn, uint64_t timeout_streak) = 0;
    virtual void HandleDisconnected(connection_it* conn) = 0;

   private:

    const Logger<field_str> logger;
    std::mutex connections_mutex;
    std::list<std::shared_ptr<Connection<T>>> connections;

    const int timeout_sec;
    int listening_fd;
    int epoll_fd;
    int timers_epoll_fd;

    void InitListeningSock(int port);
    bool HandleEvent(connection_it* it, uint32_t flags);
    bool HandleReadable(connection_it* it);

    bool RearmFd(connection_it* it, int fd, int efd) const;
    int AcceptConnection(sockaddr_in* addr, socklen_t* addr_size) const;
    void EraseClient(connection_it* it);
    void HandleNewConnection();
};
#endif