#include "block_watcher.hpp"

int main(int argc, char** argv)
{
    Logger<LogField::BlockWatcher> logger;
    if (argc < 2)
    {
        logger.Log<LogType::Critical>("Not enough parameters.");
        return -1;
    }

    std::string coin(argv[1]);

    std::vector<std::unique_ptr<ExtendedSubmission>> immature_block_submissions;
    const std::string ip = "127.0.0.1";
    const CoinConfig c = CoinConfig{.redis = RedisConfig{.redis_port = 6379}};
    RedisManager redis_manager(ip, &c);

    redis_manager.LoadImmatureBlocks(immature_block_submissions);

    for (std::unique_ptr<ExtendedSubmission>& sub : immature_block_submissions)
    {
        logger.Log<LogType::Info>(
            "Block watcher loaded immature block id: {}, hash: {}", sub->number,
            std::string_view((char*)sub->hash_hex, HASH_SIZE_HEX));
    }

    return 0;
}