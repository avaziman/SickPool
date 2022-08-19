#ifndef JOB_MANAGER_SIN_HPP_
#define JOB_MANAGER_SIN_HPP_

#include "job_manager.hpp"

class JobManagerSin : public JobManager
{
   public:
    using JobManager::JobManager;

    const job_t* GetNewJob() override;
    const job_t* GetNewJob(const std::string& json_template) override;
};

using job_manager_t = JobManagerSin;

#endif