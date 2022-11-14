#ifndef SUBMISSION_MANAGER_HPP
#define SUBMISSION_MANAGER_HPP

#include <simdjson/simdjson.h>
#include <memory>

#include "static_config.hpp"
#include "block_submission.hpp"
#include "daemon_manager_t.hpp"
#include "logger.hpp"
#include "redis/redis_manager.hpp"

template <StaticConf confs>
class BlockWatcher
{
   public:
    explicit BlockWatcher(RedisManager* redis_manager,
                          daemon_manager_t* daemon_manager);

    void CheckImmatureSubmissions();
   private:
    static constexpr std::string_view logger_field = "BlockWatcher";
    const Logger<logger_field> logger;
    std::mutex blocks_lock;
    RedisManager* redis_manager;
    daemon_manager_t* daemon_manager;

    simdjson::ondemand::parser httpParser;

    // pointer as it is not assignable for erase
    std::vector<std::unique_ptr<BlockSubmission>> immature_block_submissions;
};

#endif