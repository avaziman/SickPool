#ifndef JOB_MANAGER_SIN_HPP_
#define JOB_MANAGER_SIN_HPP_

#include "job_manager.hpp"
#include "transaction.hpp"

class JobManagerSin : public JobManager
{
   public:
    using JobManager::JobManager;
    // will be used when new transactions come on the same block
    BlockTemplateBtc blockTemplate;

    const job_t* GetNewJob() override;
    const job_t* GetNewJob(const std::string& json_template) override;

    std::size_t AddCoinbaseInput(TransactionBtc& tx, const uint32_t height);
};

using job_manager_t = JobManagerSin;

#endif