#ifndef JOB_MANAGER_VRSC_HPP_
#define JOB_MANAGER_VRSC_HPP_

#include "job_manager.hpp"
#include "job_vrsc.hpp"
class JobManagerVrsc : public JobManager<JobVrsc, Coin::VRSC>
{
   public:
    using JobManager::JobManager;
    // will be used when new transactions come on the same block

    bool GetBlockTemplate(BlockTemplateResCn& res) override;
};
#endif