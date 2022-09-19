#ifndef JOB_MANAGER_CRYPTONOTE_HPP_
#define JOB_MANAGER_CRYPTONOTE_HPP_

#include "daemon_manager_sin.hpp"
#include "daemon_responses_sin.hpp"
#include "job_manager.hpp"
#include "transaction.hpp"

class JobManagerCryptoNote : public JobManager
{
   public:
    // JobManagerCryptoNote(daemon_manager_t* daemon_manager,
    //                      PaymentManager* payment_manager,
    //                      const std::string& pool_addr)
    //     : JobManager(daemon_manager, payment_manager, pool_addr)
    // {

    // }

    // will be used when new transactions come on the same block
    BlockTemplateBtc block_template;

    const job_t* GetNewJob() /*override*/;
    const job_t* GetNewJob(const BlockTemplateRes& btemplate) /*override*/;

    std::size_t AddCoinbaseInput(TransactionBtc& tx, const uint32_t height);

   private:
    static constexpr auto hex_extra = Hexlify<coinbase_extra>();
};

using job_manager_t = JobManagerSin;

#endif