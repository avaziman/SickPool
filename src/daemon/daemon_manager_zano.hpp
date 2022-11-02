#ifndef DAEMON_MANAGER_ZANO_HPP
#define DAEMON_MANAGER_ZANO_HPP

#include "charconv"
#include "daemon_manager.hpp"
#include "daemon_responses_cryptonote.hpp"

class DaemonManagerZano : public DaemonManager
{
   public:
    using DaemonManager::DaemonManager;
    
    bool GetBlockTemplate(BlockTemplateResCn& templateRes,
                          std::string_view addr, std::string_view extra_data,
                          simdjson::ondemand::parser& parser);

    bool SubmitBlock(std::string_view block_hex,
                     simdjson::ondemand::parser& parser);

    bool Transfer(TransferResCn& transfer_res,
                  const std::vector<std::pair<std::string, int64_t>>& dests, int64_t fee,
                      simdjson::ondemand::parser& parser);
};
#endif