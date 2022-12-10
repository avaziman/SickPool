#include "job_manager_cryptonote.hpp"

std::shared_ptr<JobCryptoNote> JobManagerCryptoNote::GetNewJob(
    const BlockTemplateResCn& rpctemplate)
{
    // only add the job if it's any different from the last one
    auto last_job = GetLastJob();
    bool clean = rpctemplate.height > last_job->height;

    // if (!clean
    //     /* &&
    //     *dynamic_cast<BlockTemplateCn*>(new_job.get()) ==
    //         *dynamic_cast<BlockTemplateCn*>(last_job.get())*/)
    // {
    //     return std::shared_ptr<JobCryptoNote>{};
    // }

    auto new_job = std::make_shared<JobCryptoNote>(rpctemplate, clean);

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
