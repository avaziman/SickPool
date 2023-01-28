#ifndef JOB_MANAGER_CRYPTONOTE_HPP_
#define JOB_MANAGER_CRYPTONOTE_HPP_

#include "daemon_responses_cryptonote.hpp"
#include "job_manager.hpp"
#include "cn/serialization/serialization.h"
#include "job_cryptonote.hpp"

class JobManagerCryptoNote : public JobManager<JobCryptoNote>
{
   public:
    explicit JobManagerCryptoNote(daemon_manager_t* daemon_manager,
                         PayoutManager* payout_manager,
                         const std::string& pool_addr)
        : JobManager(daemon_manager, payout_manager, pool_addr)
    {
        BlockTemplateResCn res;
        if (!GetBlockTemplate(res))
        {
            throw std::invalid_argument("Failed to generate first job!");
        }

        // insert first job at the constructor so we don't need to make sure last_job is valid
        SetNewJob(std::make_shared<JobCryptoNote>(res, true));
    }

    // will be used when new transactions come on the same block
    std::unique_ptr<BlockTemplateCn> block_template;

    std::shared_ptr<JobCryptoNote> GetNewJob() /*override*/;

    std::shared_ptr<JobCryptoNote> GetNewJob(
        const BlockTemplateResCn& btemplate) /*override*/;

    bool GetBlockTemplate(BlockTemplateResCn& btempate);

   private:
    static constexpr auto hex_extra = Hexlify<coinbase_extra>();

};

using job_manager_t = JobManagerCryptoNote;

#endif