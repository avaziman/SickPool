#include "job_manager_cryptonote.hpp"

const JobCryptoNote* JobManagerCryptoNote::GetNewJob(
    const BlockTemplateResCn& rpctemplate)
{
    block_template = std::make_unique<BlockTemplateCn>(rpctemplate);

    auto job = std::make_unique<JobCryptoNote>(rpctemplate);

    return SetNewJob(std::move(job));
}

const JobCryptoNote* JobManagerCryptoNote::GetNewJob()
{
    BlockTemplateResCn res;
    if (!daemon_manager->GetBlockTemplate(res, pool_addr, std::string_view(hex_extra.data(), hex_extra.size()), jsonParser))
    {
        logger.Log<LogType::Critical>(
                    "Failed to get block template :(");
        // TODO: make sock err negative maybe http positive to diffrinciate
        return nullptr;
    }

    return GetNewJob(res);
}