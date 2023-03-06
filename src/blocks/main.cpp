#include "block_watcher.hpp"
#include "config_parser.hpp"

int main(int argc, char** argv)
{
    static constexpr std::string_view logger_field = "BlockWatcher";
    Logger logger{logger_field};
    if (argc < 2)
    {
        logger.Log<LogType::Critical>("Not enough parameters.");
        return -1;
    }

    Logger conf_logger{config_field_str};
    CoinConfig coinConfig;
    simdjson::padded_string json = simdjson::padded_string::load(argv[1]);
    ParseCoinConfig(json, coinConfig, conf_logger);

    std::vector<std::unique_ptr<BlockSubmission>> immature_block_submissions;
    PersistenceLayer redis_manager(coinConfig);
    DaemonManagerT daemon_manager(coinConfig.rpcs);

    if (coinConfig.symbol == "ZANO")
    {
        static constexpr StaticConf confs = ZanoStatic;
        BlockWatcher<confs> block_watcher(&redis_manager, &daemon_manager);
        block_watcher.WatchBlocks();
    }

    return 0;
}