#ifndef JOB_MANAGER_HPP
#define JOB_MANAGER_HPP
#include <simdjson/simdjson.h>

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "block_template.hpp"
#include "daemon_manager_vrsc.hpp"
#include "hash_algo.hpp"
#include "hash_wrapper.hpp"
#include "job_cryptonote.hpp"
#include "job_vrsc.hpp"
#include "logger.hpp"
#include "payout_manager.hpp"
#include "share.hpp"
#include "static_config.hpp"
template <typename Job, Coin coin>
class JobManager
{
   public:
    explicit JobManager(DaemonManagerT<coin>* daemon_manager,
                        std::string_view pool_addr)
        : pool_addr(pool_addr), daemon_manager(daemon_manager)
    {
    }
    virtual ~JobManager() = default;

    void GetFirstJob()
    {
        typename DaemonManagerT<coin>::BlockTemplateRes res;
        if (!GetBlockTemplate(res))
        {
            throw std::invalid_argument("Failed to generate first job!");
        }

        // insert first job at construction so we don't need to make sure
        // last_job is valid
        //  TODO: put in func (duplicate)
        std::shared_ptr<Job> new_job;
        if constexpr (coin == Coin::ZANO)
        {
            // ID is template hash
            new_job = std::make_shared<Job>(res, true);
        }
        else
        {
            std::string jobIdHex = fmt::format("{:08x}", job_count);

            new_job = std::make_shared<Job>(std::move(jobIdHex), res, true);
        }
        SetNewJob(std::move(new_job));
    }

    // allow concurrect reading while not being modified
    inline std::shared_ptr<Job> GetJob(std::string_view hexId)
    {
        std::shared_lock lock(jobs_mutex);
        for (const auto& job : jobs)
        {
            if (job->id == hexId)
            {
                return job;
            }
        }
        return nullptr;
    }

    inline std::shared_ptr<Job> SetNewJob(std::shared_ptr<Job> job)
    {
        job_count++;

        std::unique_lock jobs_lock(jobs_mutex);
        if (job->clean)
        {
            while (jobs.size())
            {
                // incase a job is being used
                std::shared_ptr<Job> remove_job = std::move(jobs.back());
                jobs.pop_back();
            }
        }
        last_job = jobs.emplace_back(std::move(job));
        return last_job;
    }

    inline std::shared_ptr<Job> GetLastJob()
    {
        std::shared_lock lock(jobs_mutex);
        return last_job;
    }

    bool GetBlockTemplate(DaemonManagerT<coin>::BlockTemplateRes& btempate);

    template <typename BlockTemplateResT>
    // ASSUMES THIS IS NOT THE FIRST JOB
    inline std::shared_ptr<Job> GetNewJob(const BlockTemplateResT& rpctemplate)
    {
        // only add the job if it's any different from the last one
        bool clean = rpctemplate.height > last_job->height;


        std::shared_ptr<Job> new_job{};
        if constexpr (coin == Coin::ZANO)
        {
            // ID is template hash
            new_job = std::make_shared<Job>(rpctemplate, clean);
        }
        else
        {
            std::string jobIdHex = fmt::format("{:08x}", job_count);

            new_job =
                std::make_shared<Job>(std::move(jobIdHex), rpctemplate, clean);
        }

        if (!clean && *last_job == *new_job)
        {
            return std::shared_ptr<Job>{};  // null shared ptr
        }

        return SetNewJob(std::move(new_job));
    }

    inline std::shared_ptr<Job> GetNewJob()
    {
        typename DaemonManagerT<coin>::BlockTemplateRes res;
        if (!GetBlockTemplate(res))
        {
            this->logger.template Log<LogType::Critical>(
                "Failed to get block template :(");
            return std::shared_ptr<Job>{};
        }

        return GetNewJob(res);
    }

   private:
    uint32_t job_count = 0;
    std::shared_mutex jobs_mutex;
    std::shared_ptr<Job> last_job;
    // unordered map is not thread safe for modifying and accessing different
    // elements, but a vector is, so we use other optimization (save last job)
    std::vector<std::shared_ptr<Job>> jobs;

    // multiple jobs can use the same block template, (append transactions only)
    static constexpr std::string_view field_str = "JobManager";
    const Logger logger{field_str};
    static constexpr std::string_view coinbase_extra = "SickPool.io";
    const std::string pool_addr;

    DaemonManagerT<coin>* daemon_manager;

    simdjson::ondemand::parser jsonParser;
};

#endif