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
    void Stop() noexcept;
    void Listen();

   protected:
    const CoinConfig coin_config;
    PersistenceLayer persistence_layer;
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
    inline std::size_t SendRaw(int sock, std::string_view msg) const
    {
        // dont send sigpipe
        auto res = send(sock, msg.data(), msg.size(), MSG_NOSIGNAL);

        if (res == -1)
        {
            logger.Log<LogType::Error>(
                "Failed to send on sock fd {}, errno: {} -> {}", sock, errno,
                std::strerror(errno));
        }

        return res;
    }

    //TODO: have without jsonrpc
    inline void SendRes(int sock, int64_t req_id, const RpcResult& res) const
    {
        std::string str;

        if (res.code == ResCode::OK)
        {
            str = fmt::format(
                "{{\"id\":{},\"jsonrpc\":\"2.0\",\"error\":null,\"result\":{}}}"
                "\n",
                req_id, res.msg);
        }
        else
        {
            str = fmt::format(
                "{{\"id\":{},\"jsonrpc\":\"2.0\",\"result\":"
                "null,\"error\":[{},"
                "\"{}\",null]}}\n",
                req_id, (int)res.code, res.msg);
        }

        SendRaw(sock, str);
    }

   private:
    static constexpr std::string_view field_str = "StratumBase";
    const Logger logger{field_str};

    std::vector<std::jthread> processing_threads;

    ControlServer control_server;
    std::jthread control_thread;

    void ServiceSockets(std::stop_token st);
    void HandleControlCommands(std::stop_token st);
    void HandleControlCommand(ControlCommands cmd, const char* buff);

    virtual void HandleConsumeable(connection_it* conn) = 0;
    virtual bool HandleConnected(connection_it* conn) = 0;
    virtual bool HandleTimeout(connection_it* conn, uint64_t timeout_streak) = 0;
    void HandleDisconnected(connection_it* conn) override;
};

#endif