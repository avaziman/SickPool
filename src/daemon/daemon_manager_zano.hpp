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

    bool GetAliasAddress(AliasRes& res, std::string_view alias,
                         simdjson::ondemand::parser& parser);

    bool ValidateAliasEncoding(std::string_view alias)  const {
      constexpr static bool alphabet[256] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        if (alias.size() > ALIAS_NAME_MAX_LEN) return false;

        for (const auto ch : alias)
        {
            if (!alphabet[static_cast<uint8_t>(ch)]) return false;
        }
        return true;
    }
};
#endif