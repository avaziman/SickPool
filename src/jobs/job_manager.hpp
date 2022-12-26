#ifndef JOB_MANAGER_HPP
#define JOB_MANAGER_HPP
#include <simdjson/simdjson.h>

#include <algorithm>
#include <vector>

#include "../crypto/hash_wrapper.hpp"
#include "../daemon/daemon_rpc.hpp"
#include "block_template.hpp"
#include "daemon_manager_t.hpp"
#include "job_base_btc.hpp"
#include "logger.hpp"
#include "payout_manager.hpp"
#include "share.hpp"
#include "static_config.hpp"
#include "transaction.hpp"

template <typename JobT>
class JobManager
{
   public:
    explicit JobManager(daemon_manager_t* daemon_manager,
                        PayoutManager* payout_manager,
                        const std::string& pool_addr)
        : pool_addr(pool_addr),
          daemon_manager(daemon_manager),
          payout_manager(payout_manager)
    {
    }

    // allow concurrect reading while not being modified
    inline std::shared_ptr<JobT> GetJob(std::string_view hexId)
    {
        std::shared_lock lock(jobs_mutex);
        for (const auto& job : jobs)
        {
            if (job->id == hexId)
            {
                return job;
            }
        }
        return nullptr;
    }

    inline std::shared_ptr<JobT> SetNewJob(std::shared_ptr<JobT> job)
    {
        job_count++;

        std::unique_lock jobs_lock(jobs_mutex);
        if (job->clean)
        {
            while (jobs.size())
            {
                // incase a job is being used
                std::shared_ptr<JobT> remove_job = std::move(jobs.back());
                jobs.pop_back();
            }
        }
        last_job = jobs.emplace_back(std::move(job));
        return last_job;
    }

    inline std::shared_ptr<JobT> GetLastJob()
    {
        std::shared_lock lock(jobs_mutex);
        return last_job;
    }

   protected:
    // multiple jobs can use the same block template, (append transactions only)
    static constexpr std::string_view field_str = "JobManager";
    const Logger<field_str> logger;
    static constexpr std::string_view coinbase_extra = "SickPool.io";
    const std::string pool_addr;

    daemon_manager_t* daemon_manager;
    PayoutManager* payout_manager;

    uint32_t job_count = 0;

    simdjson::ondemand::parser jsonParser;

   private:
    std::shared_mutex jobs_mutex;
    // unordered map is not thread safe for modifying and accessing different
    std::shared_ptr<JobT> last_job;
    // elements, but a vector is, so we use other optimization (save last job)
    std::vector<std::shared_ptr<JobT>> jobs;
};

#if SICK_COIN == VRSC
#include "job_manager_vrsc.hpp"
using job_manager_t = JobManagerVrsc;
#elif SICK_COIN == SIN
#include "job_manager_sin.hpp"
#else
#include "job_manager_cryptonote.hpp"

#endif

#endif