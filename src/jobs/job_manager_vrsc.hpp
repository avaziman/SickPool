#ifndef JOB_MANAGER_VRSC_HPP_
#define JOB_MANAGER_VRSC_HPP_

#include "job_manager.hpp"

class JobManagerVrsc : JobManager {
    using job_t = VerusJob;

    const job_t* GetNewJob(const std::string& json_template) override;
};

#endif