#ifndef DAEMON_MANAGER_VRSC_HPP
#define DAEMON_MANAGER_VRSC_HPP

#include "charconv"
#include "daemon_manager.hpp"
#include "daemon_manager_t.hpp"
#include "daemon_responses_cryptonote.hpp" // ORGANIZE
#include "round_share.hpp"
#include "daemon_responses_btc.hpp"

template <>
class DaemonManagerT<Coin::VRSC> : public DaemonManager
{
   public:
    using DaemonManager::DaemonManager;

    // in the order they appear, in the type they appear

    // in the order they appear, in the type they appear
    // struct BlockTemplateRes
    // {
    //     simdjson::ondemand::document doc;

    //     int32_t version;
    //     std::string_view prev_block_hash;
    //     std::vector<TxRes> transactions;
    //     int64_t coinbase_value;
    //     int64_t min_time;
    //     std::string_view bits;
    //     uint32_t height;
    // };

    struct BlockTemplateRes
    {
        simdjson::ondemand::document doc;

        int32_t version;
        std::string_view prev_block_hash;
        std::string_view final_sroot_hash;
        std::string_view solution;
        std::vector<TxRes> transactions;
        int64_t coinbase_value;
        std::string_view target;
        uint32_t min_time;
        uint32_t bits;
        uint32_t height;
    };

    struct FundRawTransactionRes
    {
        simdjson::ondemand::document doc;

        std::string_view hex;
        int changepos;
        double fee;
        std::string err;
    };

    struct SignRawTransactionRes
    {
        simdjson::ondemand::document doc;

        std::string_view hex;
        bool complete;
        std::string err;
    };

    enum class ValidationType
    {
        WORK,
        STAKE
    };

    struct BlockRes
    {
        simdjson::ondemand::document doc;

        ValidationType validation_type;
        int confirmations;
        uint32_t height;
        std::vector<std::string_view> tx_ids;
        std::string err;
    };

    bool GetBlockTemplate(BlockTemplateRes& templateRes, simdjson::ondemand::parser& parser);

    // block hash or number (both sent as string)
    bool GetBlock(BlockRes& block_res, simdjson::ondemand::parser& parser,
                  std::string_view block);

    bool ValidateAddress(ValidateAddressRes& va_res,
                         simdjson::ondemand::parser& parser,
                         std::string_view addr);

    bool FundRawTransaction(FundRawTransactionRes& fund_res,
                            simdjson::ondemand::parser& parser,
                            std::string_view raw_tx, int fee_rate,
                            std::string_view change_addr);

    bool SignRawTransaction(SignRawTransactionRes& sign_res,
                            simdjson::ondemand::parser& parser,
                            std::string_view funded_tx);

    bool SubmitBlock(const std::string_view block_hex,
                     simdjson::ondemand::parser& parser);

    bool ValidateAliasEncoding(std::string_view alias) const { return false; };
    bool GetAliasAddress(AliasRes& id_res,
                         std::string_view addr,simdjson::ondemand::parser& parser
                         );
};
#endif