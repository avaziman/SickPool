#ifndef SUBMISSION_MANAGER_HPP
#define SUBMISSION_MANAGER_HPP

#include <simdjson/simdjson.h>

#include <any>
#include <memory>

#include "block_submission.hpp"
#include "daemon_manager.hpp"
#include "redis_manager.hpp"
#include "stats_manager.hpp"

class SubmissionManager
{
   public:
    SubmissionManager(RedisManager* redis_manager,
                      DaemonManager* daemon_manager,
                      StatsManager* stats_manager)
        :
          block_number(redis_manager->GetBlockNumber()),
         redis_manager(redis_manager),
          daemon_manager(daemon_manager),
          stats_manager(stats_manager)
    {
        Logger::Log(LogType::Info, LogField::SubmissionManager,
                    "Submission manager started, block number: %u",
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

    bool AddImmatureBlock(const std::string_view chainsv,
                          const std::string_view workerFull, const job_t* job,
                          const ShareResult& shareRes, const Round& chainRound,
                          const int64_t time, double pow_fee);

    void CheckImmatureSubmissions();

   private:
    bool SubmitBlock(
        std::string_view block_hex);  // TODO: make const when we wrapped rpc
                                      // func, same for trysubmit
    const int submis_retries = 10;
    uint32_t block_number;

    RedisManager* redis_manager;
    DaemonManager* daemon_manager;
    StatsManager* stats_manager;

    simdjson::ondemand::parser httpParser;

    // pointer as it is not assignable for erase
    std::vector<std::unique_ptr<BlockSubmission>> immature_block_submissions;
};

#endif

// TODO: wrap all rpc methods we use