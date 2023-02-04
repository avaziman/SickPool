#ifndef DAEMON_MANAGER_ZANO_HPP
#define DAEMON_MANAGER_ZANO_HPP

#include "charconv"
#include "config_zano.hpp"
#include "daemon_manager.hpp"
#include "daemon_manager_t.hpp"
#include "daemon_responses_cryptonote.hpp"
#include "hash_algo.hpp"
#include "round_share.hpp"

template <>
class DaemonManagerT<Coin::ZANO> : public DaemonManager
{
   public:
    using DaemonManager::DaemonManager;

    // in the order they appear, in the type they appear
    struct BlockTemplateRes
    {
        simdjson::ondemand::document doc;

        std::string_view blob;
        uint64_t difficulty;
        uint32_t height;
        std::string_view prev_hash;
        std::string_view seed;
    };

    bool GetBlockTemplate(BlockTemplateRes& templateRes, std::string_view addr,
                          std::string_view extra_data,
                          simdjson::ondemand::parser& parser);

    bool SubmitBlock(std::string_view block_hex,
                     simdjson::ondemand::parser& parser);

    bool Transfer(TransferResCn& transfer_res, const std::vector<Payee>& dests,
                  int64_t fee, simdjson::ondemand::parser& parser);

    bool GetBlockHeaderByHash(BlockHeaderResCn& res,
                              std::string_view block_hash,
                              simdjson::ondemand::parser& parser);
    bool GetBlockHeaderByHeight(BlockHeaderResCn& res, uint32_t height,
                                simdjson::ondemand::parser& parser);

    bool GetAliasAddress(AliasRes& res, std::string_view alias,
                         simdjson::ondemand::parser& parser);

    bool ValidateAliasEncoding(std::string_view alias) const
    {
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

        if (alias.size() > ZANO_ALIAS_NAME_MAX_LEN) return false;

        return std::ranges::all_of(
            alias.cbegin(), alias.cend(),
            [](char c) { return alphabet[static_cast<uint8_t>(c)]; });
    }
};
#endif