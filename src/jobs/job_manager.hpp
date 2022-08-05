#ifndef JOB_MANAGER_HPP
#define JOB_MANAGER_HPP
#include <simdjson/simdjson.h>

#include <algorithm>

#include "../crypto/hash_wrapper.hpp"
#include "../crypto/verus_transaction.hpp"
#include "../daemon/daemon_rpc.hpp"
#include "logger.hpp"
#include "./job.hpp"
#include "./verus_job.hpp"
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

    const job_t* GetNewJob();
    const job_t* GetNewJob(const std::string& json_template);
    
    inline const job_t* GetJob(std::string_view hexId)
    {
        std::scoped_lock lock(jobs_mutex);
        for (const auto& job : jobs)
        {
            if (job.GetId() == hexId)
            {
                return &job;
            }
        }
        return nullptr;
    }

    inline const job_t* GetLastJob() { return GetJob(last_job_id_hex); }

   private:
    // multiple jobs can use the same block template, (append transactions only)

    DaemonManager* daemon_manager;
    PaymentManager* payment_manager;

    std::mutex jobs_mutex;
    // unordered map is not thread safe for modifying and accessing different
    // elements, but a vector is, so we use other optimization (save last job)
    std::vector<job_t> jobs;
    uint32_t job_count = 0;

    BlockTemplate blockTemplate;
    simdjson::ondemand::parser jsonParser;

    std::string last_job_id_hex;
    std::string coinbaseExtra = "SickPool is in the building.";
    std::string pool_addr;

    VerusTransaction GetCoinbaseTx(int64_t value, uint32_t height, int64_t,
                                   std::string_view rpc_coinbase);

    TransactionData GetCoinbaseTxData(int64_t value, uint32_t height, int64_t,
                                      std::string_view rpc_coinbase);
};

#endif