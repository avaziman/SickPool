#ifndef JOB_MANAGER_VRSC_HPP_
#define JOB_MANAGER_VRSC_HPP_

#include "job_manager.hpp"

class JobManagerVrsc : public JobManager
{
   public:
    using JobManager::JobManager;
    using JobManager::GetNewJob;
    // will be used when new transactions come on the same block
    BlockTemplateVrsc blockTemplate;

    const job_t* GetNewJob(const std::string& json_template) override;
};

using job_manager_t = JobManagerVrsc;

#endif