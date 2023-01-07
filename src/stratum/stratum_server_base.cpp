#include "stratum_server_base.hpp"

StratumBase::StratumBase(CoinConfig &&conf)
    : Server<StratumClient>(conf.stratum_port, static_cast<int>(60.0 / conf.diff_config.target_shares_rate * 2)),
      coin_config(std::move(conf)),
      persistence_layer(coin_config),
      round_manager(persistence_layer, "pow"),
      control_server(coin_config.control_port, coin_config.block_poll_interval)
{
    control_thread = std::jthread(
        std::bind_front(&StratumBase::HandleControlCommands, this));
}

StratumBase::~StratumBase()
{
    // make sure threads are pending stop as there might have been an exception
    // (Stop wasn't called)
    Stop();
    for (auto &t : processing_threads)
    {
        t.join();
    }
    control_thread.join();

    logger.Log<LogType::Info>("Stratum base destroyed.");
}

void StratumBase::Stop() noexcept
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
    const auto worker_amount = 2;
    // const auto worker_amount = std::thread::hardware_concurrency();
    processing_threads.reserve(worker_amount);

    for (auto i = 0; i < worker_amount; i++)
    {
        processing_threads.emplace_back(
            std::bind_front(&StratumBase::ServiceSockets, this));
    }

    HandleNewJob();

    for (auto &t : processing_threads)
    {
        t.join();
    }
    control_thread.join();
}

void StratumBase::HandleControlCommands(std::stop_token st)
{
    char buff[256] = {0};
    while (true)
    {
        ControlCommands cmd = control_server.GetNextCommand(buff, sizeof(buff));

        if (st.stop_requested()) break;

        HandleControlCommand(cmd, buff);
        if (buff[0])
        {
            logger.Log<LogType::Info>("Processed control command: {}", buff);
        }
    }

    logger.Log<LogType::Info>("Stopped control server on thread {}", gettid());
}

void StratumBase::HandleControlCommand(ControlCommands cmd, const char *buff)
{
    switch (cmd)
    {
        case ControlCommands::BLOCK_NOTIFY:
            HandleBlockNotify();
            break;
        case ControlCommands::NONE:
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