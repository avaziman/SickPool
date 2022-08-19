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

template <typename T>
class Server
{
   public:
    using con_it = std::list<std::unique_ptr<Connection<T>>>::iterator;

    Server(int port);
    ~Server();
    void Service();
    bool RearmSocket(con_it* it);
    int AcceptConnection(sockaddr_in *addr, socklen_t *addr_size);
    void EraseClient(con_it* it);
    void HandleNewConnection();

    virtual void HandleConsumeable(con_it* conn) = 0;
    virtual void HandleConnected(con_it* conn) = 0;
    virtual void HandleDisconnected(con_it* conn) = 0;

   private:
    std::mutex connections_mutex;
    std::list<std::unique_ptr<Connection<T>>> connections;

    int listening_fd;
    int epoll_fd;

    void InitListeningSock(int port);
    void HandleReadable(con_it* it);
};
#endif