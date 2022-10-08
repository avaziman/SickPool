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
    template <typename Template>
    JobBase(const std::string& jobId, const Template& bTemplate)
        : job_id(jobId),
          height(bTemplate.height),
          block_reward(bTemplate.coinbase_value),
          target_diff(bTemplate.target_diff),
          expected_hashes(bTemplate.expected_hashes),
          block_size(bTemplate.block_size),
          tx_count(bTemplate.tx_count)
    {
    }

    std::string_view GetId() const { return std::string_view(job_id); }

    const double target_diff;
    const double expected_hashes;
    const int64_t block_reward;
    const uint32_t height;
    const uint32_t block_size;
    const uint32_t tx_count;
    mutable std::shared_mutex job_mutex;

   protected:
    const std::string job_id;
};

#endif