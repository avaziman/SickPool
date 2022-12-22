#ifndef DAEMON_MANAGER_ZANO_HPP
#define DAEMON_MANAGER_ZANO_HPP

#include "charconv"
#include "daemon_manager.hpp"
#include "daemon_responses_cryptonote.hpp"
#include "round_share.hpp"

class DaemonManagerZano : public DaemonManager
{
   public:
    using DaemonManager::DaemonManager;

    bool GetBlockTemplate(BlockTemplateResCn& templateRes,
                          std::string_view addr, std::string_view extra_data,
                          simdjson::ondemand::parser& parser);

    bool SubmitBlock(std::string_view block_hex,
                     simdjson::ondemand::parser& parser) override;

    bool Transfer(TransferResCn& transfer_res, const std::vector<Payee>& dests,
                  int64_t fee, simdjson::ondemand::parser& parser);

    bool GetBlockHeaderByHash(BlockHeaderResCn& res,
                              std::string_view block_hash,
                              simdjson::ondemand::parser& parser);
    bool GetBlockHeaderByHeight(BlockHeaderResCn& res, uint32_t height,
                                simdjson::ondemand::parser& parser);
};
#endif