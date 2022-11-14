#include "stratum_server_base.hpp"

StratumBase::StratumBase(CoinConfig &&conf)
    : Server<StratumClient>(conf.stratum_port),
      coin_config(std::move(conf)),
      redis_manager("127.0.0.1", &conf),
      diff_manager(&clients, &clients_mutex, coin_config.target_shares_rate),
      round_manager(redis_manager, "pow")
{
    control_server.Start(coin_config.control_port);
    control_thread = std::jthread(
        std::bind_front(&StratumBase::HandleControlCommands, this));
}

StratumBase::~StratumBase()
{
    // no need to stop, as stopping has caused this destruction...
    // Stop();
    logger.Log<LogType::Info>("Stratum base destroyed.");
}

void StratumBase::Stop()
{
    logger.Log<LogType::Info>("Stopping socket servicing...");

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

    logger.Log<LogType::Info>("Stopped servicing sockets on thread {}",
                              gettid());
}

void StratumBase::Listen()
{
    // const auto worker_amount = 2;
    const auto worker_amount = std::thread::hardware_concurrency();
    processing_threads.reserve(worker_amount);

    for (auto i = 0; i < worker_amount; i++)
    {
        processing_threads.emplace_back(
            std::bind_front(&StratumBase::ServiceSockets, this));
    }

    HandleBlockNotify();

    for (auto &t : processing_threads)
    {
        t.join();
    }
    control_thread.join();
}

void StratumBase::HandleControlCommands(std::stop_token st)
{
    char buff[256] = {0};
    while (!st.stop_requested())
    {
        ControlCommands cmd = control_server.GetNextCommand(buff, sizeof(buff));

        if (cmd == ControlCommands::NONE) continue;

        HandleControlCommand(cmd, buff);
        logger.Log<LogType::Info>("Processed control command: {}", buff);
    }

    logger.Log<LogType::Info>("Stopped control server on thread {}",
                              gettid());
}

void StratumBase::HandleControlCommand(ControlCommands cmd, const char* buff)
{
    switch (cmd)
    {
        case ControlCommands::BLOCK_NOTIFY:
            HandleBlockNotify();
            break;
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