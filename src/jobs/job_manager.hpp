#ifndef JOB_MANAGER_HPP
#define JOB_MANAGER_HPP
#include <simdjson/simdjson.h>

#include <algorithm>

#include "static_config.hpp"
#include "share.hpp"
#include "../crypto/hash_wrapper.hpp"
#include "../daemon/daemon_rpc.hpp"
#include "logger.hpp"
#include "transaction.hpp"
#include "./job.hpp"
#include "block_template.hpp"
#include "payment_manager.hpp"
#include "daemon_manager.hpp"

class JobManager
{
   public:
    JobManager(DaemonManager* daemon_manager, PaymentManager* payment_manager,
               const std::string& pool_addr)
        : daemon_manager(daemon_manager),
          payment_manager(payment_manager), pool_addr(pool_addr)
    {
    }

    virtual const job_t* GetNewJob();
    virtual const job_t* GetNewJob(const std::string& json_template) = 0;
    
    inline const job_t* GetJob(std::string_view hexId)
    {
        std::scoped_lock lock(jobs_mutex);
        for (const auto& job : jobs)
        {
            if (job->GetId() == hexId)
            {
                return job.get();
            }
        }
        return nullptr;
    }

    inline const job_t* GetLastJob() { return GetJob(last_job_id_hex); }

   protected:
    // multiple jobs can use the same block template, (append transactions only)

    DaemonManager* daemon_manager;
    PaymentManager* payment_manager;

    std::mutex jobs_mutex;
    // unordered map is not thread safe for modifying and accessing different
    // elements, but a vector is, so we use other optimization (save last job)
    std::vector<std::unique_ptr<job_t>> jobs;
    uint32_t job_count = 0;

    simdjson::ondemand::parser jsonParser;

    static constexpr std::string_view coinbase_extra = "SickPool.io";
    std::string last_job_id_hex;
    std::string pool_addr;

    virtual transaction_t GetCoinbaseTx(int64_t value, uint32_t height,
                                const std::vector<Output>& extra_outputs);

    std::size_t GetCoinbaseTxData(TransactionData& res,
                                              transaction_t& coinbaseTx);
};

#if COIN == VRSC
#include "job_manager_vrsc.hpp"
#elif COIN == SIN
#include "job_manager_sin.hpp"
#endif

#endif