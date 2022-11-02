#ifndef SERVER_HPP_
#define SERVER_HPP_

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
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
static constexpr std::string_view field_str = "Server";

template <typename T>
class Server
{
   public:
    using connection_it = std::list<std::shared_ptr<Connection<T>>>::iterator;

    Server(int port);
    ~Server();
    void Service();
    bool RearmSocket(connection_it* it);
    int AcceptConnection(sockaddr_in *addr, socklen_t *addr_size);
    void EraseClient(connection_it* it);
    void HandleNewConnection();

    virtual void HandleConsumeable(connection_it* conn) = 0;
    virtual void HandleConnected(connection_it* conn) = 0;
    virtual void HandleDisconnected(connection_it* conn) = 0;

   private:

    Logger<field_str> logger;
    std::mutex connections_mutex;
    std::list<std::shared_ptr<Connection<T>>> connections;

    int listening_fd;
    int epoll_fd;

    void InitListeningSock(int port);
    void HandleReadable(connection_it* it);
};
#endif