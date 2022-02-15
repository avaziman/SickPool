#ifndef JOB_HPP
#define JOB_HPP

#include <sstream>
#include "../crypto/block.hpp"

class Job : public Block
{
   public:
    Job(uint32_t id, Block block);
    std::string GetId();
    bool ShouldCleanJobs();
    double GetTargetDiff() { return target_diff; }

    virtual int GetNotifyMessage(char* message, size_t size) = 0;

   private:
    uint32_t job_id;
    bool clean_jobs;
    double target_diff;
};

#endif