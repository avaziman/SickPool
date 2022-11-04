#ifndef JOB_MANAGER_CRYPTONOTE_HPP_
#define JOB_MANAGER_CRYPTONOTE_HPP_

#include "daemon_responses_cryptonote.hpp"
#include "job_manager.hpp"
#include "transaction.hpp"
#include "cn/serialization/serialization.h"

class JobManagerCryptoNote : public JobManager
{
   public:
    using JobManager::JobManager;
    // JobManagerCryptoNote(daemon_manager_t* daemon_manager,
    //                      PaymentManager* payment_manager,
    //                      const std::string& pool_addr)
    //     : JobManager(daemon_manager, payment_manager, pool_addr)
    // {

    // }

    // will be used when new transactions come on the same block
    BlockTemplateCn block_template;

    const job_t* GetNewJob() /*override*/;
    const job_t* GetNewJob(const BlockTemplateResCn& btemplate) /*override*/;

   private:
    static constexpr auto hex_extra = Hexlify<coinbase_extra>();

};

using job_manager_t = JobManagerCryptoNote;

#endif