#include "job_manager.hpp"
template class JobManager<Job<StratumProtocol::CN>, Coin::ZANO>;
template class JobManager<Job<StratumProtocol::ZEC>, Coin::VRSC>;

template <>
bool JobManager<JobCryptoNote, Coin::ZANO>::GetBlockTemplate(
    BlockTemplateResCn& res)
{
    if (static constexpr auto hex_extra = Hexlify<coinbase_extra>();
        !daemon_manager->GetBlockTemplate(
            res, pool_addr,
            std::string_view(hex_extra.data(), hex_extra.size()), jsonParser))
    {
        return false;
    }
    return true;
}

template <typename Job, Coin coin>
bool JobManager<Job, coin>::GetBlockTemplate(
    DaemonManagerT<coin>::BlockTemplateRes& res)
{
    if (!daemon_manager->GetBlockTemplate(res, jsonParser))
    {
        return false;
    }
    return true;
}
