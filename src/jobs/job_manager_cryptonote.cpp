#include "job_manager_cryptonote.hpp"

std::shared_ptr<JobCryptoNote> JobManagerCryptoNote::GetNewJob(
    const BlockTemplateResCn& rpctemplate)
{
    auto new_job = std::make_shared<JobCryptoNote>(rpctemplate);

    // only add the job if it's any different from the last one
    if (auto last_job = GetLastJob();
        new_job->height == last_job->height/* &&
        *dynamic_cast<BlockTemplateCn*>(new_job.get()) ==
            *dynamic_cast<BlockTemplateCn*>(last_job.get())*/)
    {
        return std::shared_ptr<JobCryptoNote>{};
    }

    return SetNewJob(std::move(new_job));
}

std::shared_ptr<JobCryptoNote> JobManagerCryptoNote::GetNewJob()
{
    BlockTemplateResCn res;
    if (!GetBlockTemplate(res))
    {
        return std::shared_ptr<JobCryptoNote>{};
    }

    return GetNewJob(res);
}

bool JobManagerCryptoNote::GetBlockTemplate(BlockTemplateResCn& res)
{
    if (!daemon_manager->GetBlockTemplate(
            res, pool_addr,
            std::string_view(hex_extra.data(), hex_extra.size()), jsonParser))
    {
        logger.Log<LogType::Critical>("Failed to get block template :(");
        return false;
    }
    return true;
}
