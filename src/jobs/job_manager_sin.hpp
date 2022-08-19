#ifndef JOB_MANAGER_SIN_HPP_
#define JOB_MANAGER_SIN_HPP_

#include "job_manager.hpp"

class JobManagerSin : JobManager
{
    const job_t* GetNewJob(const std::string& json_template) override;
};

#endif