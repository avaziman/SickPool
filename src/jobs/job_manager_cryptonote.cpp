#include "job_manager_cryptonote.hpp"

const job_t* JobManagerCryptoNote::GetNewJob(
    const BlockTemplateResCn& rpctemplate)
{
    block_template = BlockTemplateCn(rpctemplate);
    std::string jobIdHex = fmt::format("{:08x}", job_count);

    auto job =
        std::make_unique<job_t>(jobIdHex, std::move(block_template));

    return SetNewJob(std::move(job));
}

const job_t* JobManagerCryptoNote::GetNewJob()
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