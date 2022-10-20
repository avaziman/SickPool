#ifndef SUBMISSION_MANAGER_HPP
#define SUBMISSION_MANAGER_HPP

#include <simdjson/simdjson.h>

#include <any>
#include <memory>

#include "block_submission.hpp"
#include "daemon_manager_t.hpp"
#include "logger.hpp"
#include "redis/redis_manager.hpp"

class BlockWatcher
{
   public:
    BlockWatcher(RedisManager* redis_manager,
                      daemon_manager_t* daemon_manager)
        : redis_manager(redis_manager),
          daemon_manager(daemon_manager)
    {
        redis_manager->LoadImmatureBlocks(immature_block_submissions);

        for (std::unique_ptr<ExtendedSubmission>& sub : immature_block_submissions) {
            logger.Log<LogType::Info>(
                        "Block watcher loaded immature block id: {}, hash: {}",
                        sub->number, std::string_view((char*)sub->hash_hex, HASH_SIZE_HEX));
        }
    }

    void CheckImmatureSubmissions();
   private:
    Logger<LogField::BlockWatcher> logger;
    std::mutex blocks_lock;
    RedisManager* redis_manager;
    daemon_manager_t* daemon_manager;

    simdjson::ondemand::parser httpParser;

    // pointer as it is not assignable for erase
    std::vector<std::unique_ptr<ExtendedSubmission>> immature_block_submissions;
};

#endif

// TODO: load all immature blocks