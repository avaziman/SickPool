#ifndef SUBMISSION_MANAGER_HPP
#define SUBMISSION_MANAGER_HPP

#include <simdjson/simdjson.h>
#include <memory>

#include "redis_manager.hpp"
#include "static_config.hpp"

#include "daemon_manager_t.hpp"
#include "logger.hpp"
#include "redis_block.hpp"

template <StaticConf confs>
class BlockWatcher
{
   public:
    explicit BlockWatcher(const PersistenceLayer* pl,
                          daemon_manager_t* daemon_manager);

    void CheckImmatureSubmissions();
    void WatchBlocks();

   private:
    static constexpr std::string_view logger_field = "BlockWatcher";
    const Logger logger{logger_field};
    std::mutex blocks_lock;
    PersistenceBlock persistence_block;
    daemon_manager_t* daemon_manager;

    simdjson::ondemand::parser httpParser;

    // pointer as it is not assignable for erase
    std::vector<BlockOverview> immature_block_submissions;
};

#endif