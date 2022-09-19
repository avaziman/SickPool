#include "daemon_manager_zano.hpp"

bool DaemonManagerZano::GetBlockTemplate(BlockTemplateRes& templateRes, std::string_view addr, std::string extra_data,
                                        simdjson::ondemand::parser& parser)
{
    using namespace simdjson;
    using namespace std::string_view_literals;

    std::string result_body;
    int res_code = SendRpcReq(result_body, 1, "getblocktemplate"sv,
                              DaemonRpc::GetParamsStr(
                                 std::make_pair("wallet_address"sv, addr), 
                                 std::make_pair("extra_text"sv, extra_data)));

    if (res_code != 200)
    {
        Logger::Log(LogType::Warn, LogField::DaemonManager,
                    "Failed to getblocktemplate, rescode: {}, response: {}",
                    res_code, result_body);
        return false;
    }

    ondemand::object res;
    try
    {
        templateRes.doc = parser.iterate(result_body.data(), result_body.size(),
                                         result_body.capacity());

        res = templateRes.doc["result"].get_object();

        templateRes.version = static_cast<int32_t>(res["version"].get_int64());

        templateRes.prev_block_hash = res["previousblockhash"].get_string();

        ondemand::array txs = res["transactions"].get_array();

        for (auto tx : txs)
        {
            std::string_view tx_data_hex = tx["data"].get_string();
            std::string_view tx_hash_hex = tx["hash"].get_string();
            int64_t fee = tx["fee"].get_int64();
            templateRes.transactions.emplace_back(
                TxRes(tx_data_hex, tx_hash_hex, fee));
        }

        templateRes.coinbase_value = res["coinbasevalue"].get_int64();

        templateRes.min_time = res["mintime"].get_int64();
        templateRes.bits = res["bits"].get_string();

        templateRes.height = static_cast<uint32_t>(res["height"].get_int64());

        auto dev_fees = res["devfee"].get_array();
        for (auto df : dev_fees)
        {
            // std::string_view address = df["address"].get_string();
            std::string_view data_hex = df["script"]["hex"].get_string();
            int64_t value = df["value"].get_int64();

            templateRes.infinity_nodes.emplace_back(OutputRes(data_hex, value));
        }

        auto infinitynodes = res["infinitynodes"].get_array();
        for (auto infnode : infinitynodes)
        {
            // std::string_view address = infnode["address"].get_string();
            std::string_view data_hex = infnode["script"]["hex"].get_string();
            int64_t value = infnode["value"].get_int64();

            templateRes.infinity_nodes.emplace_back(OutputRes(data_hex, value));
        }
    }
    catch (const simdjson_error& err)
    {
        Logger::Log(LogType::Warn, LogField::DaemonManager,
                    "Failed to parse signrawtransaction response: {}",
                    err.what());
        return false;
    }

    return true;
}
