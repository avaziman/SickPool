#ifndef JOB_MANAGER_HPP
#define JOB_MANAGER_HPP
#include <simdjson/simdjson.h>

#include <algorithm>

#include "../crypto/hash_wrapper.hpp"
#include "../daemon/daemon_rpc.hpp"
#include "block_template.hpp"
#include "daemon_manager_t.hpp"
#include "job_base_btc.hpp"
#include "logger.hpp"
#include "payment_manager.hpp"
#include "share.hpp"
#include "static_config.hpp"
#include "transaction.hpp"
using namespace std::string_view_literals;

class JobManager
{
   public:
    JobManager(daemon_manager_t* daemon_manager,
               PaymentManager* payment_manager, const std::string& pool_addr)
        : daemon_manager(daemon_manager),
          payment_manager(payment_manager),
          pool_addr(pool_addr)
    {
    }

    // virtual const job_t* GetNewJob();
    // virtual const job_t* GetNewJob(const std::string& json_template) = 0;

    // allow concurrect reading while not being modified
    inline const job_t* GetJob(std::string_view hexId)
    {
        std::shared_lock<std::shared_mutex> lock(jobs_mutex);
        for (const auto& job : jobs)
        {
            if (job->GetId() == hexId)
            {
                return job.get();
            }
        }
        return nullptr;
    }

    inline job_t* SetNewJob(std::unique_ptr<job_t> job)
    {
        last_job_id_hex = job->GetId();
        job_count++;

        std::unique_lock jobs_lock(jobs_mutex);
        while (jobs.size())
        {
            // incase a job is being used
            auto remove_job = std::move(jobs.back());
            std::unique_lock job_lock(remove_job->job_mutex);
            jobs.pop_back();
        }
        return jobs.emplace_back(std::move(job)).get();
    }

    inline const job_t* GetLastJob() { return GetJob(last_job_id_hex); }

   protected:
    // multiple jobs can use the same block template, (append transactions only)
    static constexpr std::string_view field_str = "JobManager";
    const Logger<field_str> logger;
    daemon_manager_t* daemon_manager;
    PaymentManager* payment_manager;

    std::shared_mutex jobs_mutex;
    // unordered map is not thread safe for modifying and accessing different
    // elements, but a vector is, so we use other optimization (save last job)
    std::vector<std::unique_ptr<job_t>> jobs;
    uint32_t job_count = 0;

    simdjson::ondemand::parser jsonParser;

    static constexpr std::string_view coinbase_extra = "SickPool.io"sv;

    std::string last_job_id_hex;
    const std::string pool_addr;

    virtual transaction_t GetCoinbaseTx(int64_t value, uint32_t height,
                                        std::string_view rpc_cb);
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