#ifndef JOB_MANAGER_SIN_HPP_
#define JOB_MANAGER_SIN_HPP_

#include "job_manager.hpp"
#include "transaction.hpp"
#include "daemon_manager_sin.hpp"
#include "daemon_responses_sin.hpp"

class JobManagerSin : public JobManager
{
   public:
    using JobManager::JobManager;
    // will be used when new transactions come on the same block
    BlockTemplateBtc block_template = BlockTemplateBtc();

    const JobT* GetNewJob() /*override*/;
    const JobT* GetNewJob(const BlockTemplateResSin& btemplate) /*override*/;

    std::size_t GetCoinbaseTx(TransactionBtc& coinbase_tx,
                              const BlockTemplateResSin& rpctemplate);
    std::size_t AddCoinbaseInput(TransactionBtc& tx, const uint32_t height);
};

using job_manager_t = JobManagerSin;

#endif