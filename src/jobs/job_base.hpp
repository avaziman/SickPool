#ifndef JOB_BASE_HPP_
#define JOB_BASE_HPP_

#include <ctime>
#include <iomanip>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <vector>

#include "block_template.hpp"
#include "merkle_tree.hpp"
#include "share.hpp"
#include "static_config.hpp"
#include "utils.hpp"

class JobBase
{
   public:
    explicit JobBase(std::string&& jobId)
        : id(std::move(jobId))
    {
    }

    // const double target_diff;
    // const double expected_hashes;
    // const int64_t block_reward;
    // const uint32_t height;
    // const uint32_t block_size;
    // const uint32_t tx_count;
    // locked when a job is being read from, so it won't be removed.
    mutable std::shared_mutex job_mutex;
    const std::string id;

   protected:
};

#endif