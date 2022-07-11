#ifndef SUBMISSION_MANAGER_HPP
#define SUBMISSION_MANAGER_HPP

#include <simdjson/simdjson.h>

#include <any>
#include <memory>

#include "block_submission.hpp"
#include "daemon_manager.hpp"
#include "stats/stats_manager.hpp"

class SubmissionManager
{
   public:
    SubmissionManager(RedisManager* redis_manager,
                      DaemonManager* daemon_manager,
                      RoundManager* stats_manager)
        : redis_manager(redis_manager),
          daemon_manager(daemon_manager),
          round_manager(stats_manager)
    {
        SubmissionManager::block_number = redis_manager->GetBlockNumber();
        Logger::Log(LogType::Info, LogField::SubmissionManager,
                    "Submission manager started, block number: {}",
                    block_number);
    }

    inline bool TrySubmit(const std::string_view chain,
                          const std::string_view block_hex)
    {
        bool added = false;
        for (int i = 0; i < submis_retries; ++i)
        {
            added = SubmitBlock(block_hex);
            if (added)
            {
                break;
            }
        }
        return added;
    }

    bool AddImmatureBlock(std::unique_ptr<BlockSubmission> submission,
                          double pow_fee);

    void CheckImmatureSubmissions();

    static uint32_t block_number;

   private:
    bool SubmitBlock(
        std::string_view block_hex);  // TODO: make const when we wrapped rpc
                                      // func, same for trysubmit
    int64_t last_matured_time = 0;
    const int submis_retries = 10;

    std::mutex blocks_lock;
    RedisManager* redis_manager;
    DaemonManager* daemon_manager;
    RoundManager* round_manager;

    simdjson::ondemand::parser httpParser;

    // pointer as it is not assignable for erase
    std::vector<std::unique_ptr<BlockSubmission>> immature_block_submissions;
};

#endif

// TODO: wrap all rpc methods we use