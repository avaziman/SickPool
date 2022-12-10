#ifndef STRATUM_SERVER_BASE_HPP_
#define STRATUM_SERVER_BASE_HPP_
#include "control_server.hpp"
#include "logger.hpp"
#include "redis_manager.hpp"
#include "server.hpp"
#include "share.hpp"
#include "stats_manager.hpp"

class StratumBase : public Server<StratumClient>
{
   public:
    explicit StratumBase(CoinConfig&& conf);
    virtual ~StratumBase();
    void Stop();
    void Listen();

   protected:
    const CoinConfig coin_config;
    RedisManager redis_manager;
    DifficultyManager diff_manager;
    RoundManager round_manager;

    // O(log n) delete + insert
    // saving the pointer in epoll gives us O(1) access!
    // allows us to sort connections by hashrate to minimize loss
    std::map<std::shared_ptr<Connection<StratumClient>>, double> clients;
    std::shared_mutex clients_mutex;

    virtual void HandleBlockNotify() = 0;
    virtual void HandleNewJob() = 0;

    virtual void DisconnectClient(
        const std::shared_ptr<Connection<StratumClient>> conn_ptr) = 0;

    inline std::stop_token GetStopToken() const { return control_thread.get_stop_token(); }
    inline std::size_t SendRaw(int sock, const char* data,
                               std::size_t len) const
    {
        // dont send sigpipe
        auto res = send(sock, data, len, MSG_NOSIGNAL);

        if (res == -1)
        {
            logger.Log<LogType::Error>(
                "Failed to send on sock fd {}, errno: {} -> {}", sock, errno,
                std::strerror(errno));
        }

        return res;
    }

    inline void SendRes(int sock, int req_id, const RpcResult& res) const
    {
        char buff[512];
        size_t len = 0;

        if (res.code == ResCode::OK)
        {
            len =
                fmt::format_to_n(buff, sizeof(buff),
                                 "{{\"id\":{},\"result\":{},\"error\":null}}\n",
                                 req_id, res.msg)
                    .size;
        }
        else
        {
            len = fmt::format_to_n(buff, sizeof(buff),
                                   "{{\"id\":{},\"result\":null,\"error\":[{},"
                                   "\"{}\",null]}}\n",
                                   req_id, (int)res.code, res.msg)
                      .size;
        }

        SendRaw(sock, buff, len);
    }

   private:
    static constexpr std::string_view field_str = "StratumBase";
    const Logger<field_str> logger;
    
    std::vector<std::jthread> processing_threads;

    ControlServer control_server;
    std::jthread control_thread;

    void ServiceSockets(std::stop_token st);
    void HandleControlCommands(std::stop_token st);
    void HandleControlCommand(ControlCommands cmd, const char* buff);

    virtual void HandleConsumeable(connection_it* conn) = 0;
    virtual bool HandleConnected(connection_it* conn) = 0;
    void HandleDisconnected(connection_it* conn) override;
};

#endif