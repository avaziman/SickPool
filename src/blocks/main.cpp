#include "block_watcher.hpp"
#include "config_parser.hpp"

int main(int argc, char** argv)
{
    static constexpr std::string_view logger_field = "BlockWatcher";
    Logger<logger_field> logger;
    if (argc < 2)
    {
        logger.Log<LogType::Critical>("Not enough parameters.");
        return -1;
    }

    Logger<config_field_str> conf_logger;
    CoinConfig coinConfig;
    simdjson::padded_string json = simdjson::padded_string::load(argv[1]);
    ParseCoinConfig(json, coinConfig, conf_logger);

    std::vector<std::unique_ptr<ExtendedSubmission>> immature_block_submissions;
    const std::string ip = "127.0.0.1";
    RedisManager redis_manager(ip, &coinConfig);
    daemon_manager_t daemon_manager(coinConfig.rpcs);
    BlockWatcher block_watcher(&redis_manager, &daemon_manager);

    block_watcher.CheckImmatureSubmissions();

    return 0;
}