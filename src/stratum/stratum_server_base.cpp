#include "stratum_server_base.hpp"

StratumBase::StratumBase(CoinConfig &&conf)
    : Server<StratumClient>(conf.stratum_port),
      coin_config(std::move(conf)),
      redis_manager("127.0.0.1", &conf),
      diff_manager(&clients, &clients_mutex, coin_config.target_shares_rate),
      round_manager(redis_manager, "pow"),
      stats_manager(&redis_manager, &diff_manager, &round_manager, &conf.stats)
{
    stats_thread =
        std::jthread(std::bind_front(&StatsManager::Start, &stats_manager));

    control_server.Start(coin_config.control_port);
    control_thread = std::jthread(
        std::bind_front(&StratumBase::HandleControlCommands, this));
}

StratumBase::~StratumBase()
{
    Stop();
    logger.Log<LogType::Info>("Stratum base destroyed.");
}

void StratumBase::Stop()
{
    stats_thread.request_stop();
    control_thread.request_stop();
    for (auto &t : processing_threads)
    {
        t.request_stop();
    }
}

void StratumBase::ServiceSockets(std::stop_token st)
{
    logger.Log<LogType::Info>("Starting servicing sockets on thread {}",
                              gettid());

    while (!st.stop_requested())
    {
        Service();
    }
}

void StratumBase::Listen()
{
    // const auto worker_amount = 2;
    const auto worker_amount = std::thread::hardware_concurrency();
    processing_threads.reserve(worker_amount);

    for (auto i = 0; i < worker_amount; i++)
    {
        processing_threads.emplace_back(
            std::bind_front(&StratumBase::ServiceSockets, (StratumBase *)this));
    }

    HandleBlockNotify();

    for (auto &t : processing_threads)
    {
        t.join();
    }
    stats_thread.join();
    control_thread.join();
}

void StratumBase::HandleControlCommands(std::stop_token st)
{
    char buff[256] = {0};
    while (!st.stop_requested())
    {
        ControlCommands cmd = control_server.GetNextCommand(buff, sizeof(buff));
        HandleControlCommand(cmd, buff);
        logger.Log<LogType::Info>("Processed control command: {}", buff);
    }
}

void StratumBase::HandleControlCommand(ControlCommands cmd, char buff[])
{
    switch (cmd)
    {
        case ControlCommands::BLOCK_NOTIFY:
            HandleBlockNotify();
            break;
        // case ControlCommands::WALLET_NOTFIY:
        // {
        //     // format: %b%s%w (block hash, txid, wallet address)
        //     WalletNotify *wallet_notify =
        //         reinterpret_cast<WalletNotify *>(buff + 2);
        //     // HandleWalletNotify(wallet_notify);
        //     break;
        // }
        default:
            logger.Log<LogType::Warn>("Unknown control command {} received.",
                                      (int)cmd);
            break;
    }
}

void StratumBase::HandleDisconnected(connection_it *conn)
{
    auto conn_ptr = *(*conn);
    DisconnectClient(conn_ptr);
}

void StratumBase::DisconnectClient(
    const std::shared_ptr<Connection<StratumClient>> conn_ptr)
{
    auto sock = conn_ptr->sock;
    stats_manager.PopWorker(conn_ptr->ptr->GetFullWorkerName(),
                            conn_ptr->ptr->GetAddress());
    std::unique_lock lock(clients_mutex);
    clients.erase(conn_ptr);

    logger.Log<LogType::Info>("Stratum client disconnected. sock: {}", sock);
}