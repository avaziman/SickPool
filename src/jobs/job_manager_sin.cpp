#include "static_config.hpp"
#if COIN == SIN
#include "job_manager_sin.hpp"

const job_t* JobManagerSin::GetNewJob(const std::string& json_template)
{
    using namespace simdjson;
    try
    {
        ondemand::document doc =
            jsonParser.iterate(json_template.data(), json_template.size(),
                               json_template.capacity());

        blockTemplate = BlockTemplateBtc();
        ondemand::object res = doc["result"].get_object();

        // this must be in the order they appear in the result for simdjson
        blockTemplate.version =
            static_cast<int32_t>(res["version"].get_int64());
        blockTemplate.prev_block_hash = res["previousblockhash"].get_string();
        // can't iterate after we get the string_view
        ondemand::array txs = res["transactions"].get_array();

        int64_t additional_fee = 0;

        bool includes_payment = payment_manager->ShouldIncludePayment(
            blockTemplate.prev_block_hash);

        if (includes_payment)
        {
            blockTemplate.tx_list.AddTxData(payment_manager->pending_tx->td);
            additional_fee += payment_manager->pending_tx->td.fee;
        }

        // for (auto tx : txs)
        // {
        //     std::string_view tx_data_hex = tx["data"].get_string();
        //     std::string_view tx_hash_hex = tx["hash"].get_string();
        //     TransactionData td(tx_data_hex, tx_hash_hex);
        //     td.fee = tx["fee"].get_double();

        //     // std::cout << "tx data: " << td.data_hex << std::endl;

        //     int txSize = td.data_hex.size() / 2;
        //     td.data = std::vector<uint8_t>(txSize);
        //     Unhexlify(td.data.data(), td.data_hex.data(),
        //     td.data_hex.size()); Unhexlify(td.hash, tx_hash_hex.data(),
        //     tx_hash_hex.size());  // hash
        //     // is reversed std::reverse(td.hash, td.hash + 32);

        //     if (!blockTemplate.tx_list.AddTxData(td))
        //     {
        //         Logger::Log(LogType::Warn, LogField::JobManager,
        //                     "Block template is full! block size is {} bytes",
        //                     blockTemplate.tx_list.byteCount);
        //         break;
        //     }
        // }

        blockTemplate.coinbase_value = res["coinbasevalue"].get_int64();

        blockTemplate.min_time = res["mintime"].get_int64();
        std::string_view bits_sv = res["bits"].get_string();
        blockTemplate.bits = HexToUint(bits_sv.data(), BITS_SIZE * 2);

        blockTemplate.height = static_cast<uint32_t>(res["height"].get_int64());

        TransactionBtc coinbase_tx;
        // reserve for cb
        coinbase_tx.AddOutput(Output());

        std::size_t coinb1_pos =
            AddCoinbaseInput(coinbase_tx, blockTemplate.height);

        auto dev_fees = res["devfee"].get_array();
        for (auto df : dev_fees)
        {
            // std::string_view address = df["address"].get_string();
            std::string_view data_hex = df["script"]["hex"].get_string();
            int64_t value = df["value"].get_int64();
            coinbase_tx.AddOutput(Output(value, data_hex));
            blockTemplate.coinbase_value -= value;
        }

        auto infinitynodes = res["infinitynodes"].get_array();
        for (auto infnode : infinitynodes)
        {
            // std::string_view address = infnode["address"].get_string();
            std::string_view data_hex = infnode["script"]["hex"].get_string();
            int64_t value = infnode["value"].get_int64();
            coinbase_tx.AddOutput(Output(value, data_hex));
            blockTemplate.coinbase_value -= value;
        }

        uint64_t total_coinbase_reward =
            blockTemplate.coinbase_value + additional_fee;
        std::vector<uint8_t> reward_script;
        Transaction::GetP2PKHScript(pool_addr, reward_script);
        coinbase_tx.SwitchOutput(0,
                                 Output(total_coinbase_reward, reward_script));

        TransactionData cb_txd;
        coinbase_tx.GetBytes(cb_txd.data);

        char cb_data_hex[cb_txd.data.size() * 2];
        Hexlify(cb_data_hex, cb_txd.data.data(), cb_txd.data.size());
        cb_txd.data_hex = std::string_view(cb_data_hex, sizeof(cb_data_hex));

        Logger::Log(LogType::Info, LogField::JobManager, "Coinbase tx: {}",
                    cb_txd.data_hex);

        std::size_t coinb2_pos =
            coinb1_pos + EXTRANONCE_SIZE + EXTRANONCE2_SIZE;

        blockTemplate.coinb1 =
            std::span<uint8_t>(cb_txd.data.data(), coinb1_pos);
        blockTemplate.coinb2 = std::span<uint8_t>(
            cb_txd.data.data() + coinb2_pos, cb_txd.data.size() - coinb2_pos);

        blockTemplate.tx_list.AddCoinbaseTxData(cb_txd);
        // TransactionData coinbaseTx = GetCoinbaseTxData(
        //     blockTemplate.coinbase_value + additional_fee,
        //     blockTemplate.height, blockTemplate.min_time, cb_outputs);

        // we need to hexlify here otherwise hex will be garbage
        // char coinbaseHex[coinbaseTx.data.size() * 2];
        // Hexlify(coinbaseHex, coinbaseTx.data.data(), coinbaseTx.data.size());
        // coinbaseTx.data_hex =
        //     std::string_view(coinbaseHex, sizeof(coinbaseHex));
        // blockTemplate.tx_list.AddCoinbaseTxData(coinbaseTx);

        std::string jobIdHex = fmt::format("{:08x}", job_count);

        auto job =
            std::make_unique<job_t>(jobIdHex, blockTemplate, includes_payment);

        last_job_id_hex = jobIdHex;
        job_count++;

        std::scoped_lock jobs_lock(jobs_mutex);
        jobs.clear();

        jobs.emplace_back(std::move(job));
        return jobs.back().get();
    }
    catch (const simdjson::simdjson_error& err)
    {
        Logger::Log(LogType::Critical, LogField::JobManager,
                    "Failed to parse block template: {}, json: {}", err.what(),
                    json_template);
    }
    return nullptr;
}

const job_t* JobManagerSin::GetNewJob()
{
    using namespace simdjson;
    using namespace std::string_view_literals;

    std::string json;
    int resCode = daemon_manager->SendRpcReq(json, 1, "getblocktemplate"sv,
                                             "[{\"rules\":[\"segwit\"]}]"sv);

    if (resCode != 200)
    {
        Logger::Log(LogType::Critical, LogField::JobManager,
                    "Failed to get block template, http code: {}, response: {}",
                    strerror(resCode), json);
        // TODO: make sock err negative maybe http positive to diffrinciate
        return nullptr;
    }

    return GetNewJob(json);
}

std::size_t JobManagerSin::AddCoinbaseInput(TransactionBtc& tx,
                                            const uint32_t height)
{
    unsigned char prevTxIn[HASH_SIZE] = {0};  // null last input
    std::vector<uint8_t> height_script = GenNumScript(height);
    std::vector<uint8_t> signature(1 + height_script.size() +
                                   coinbase_extra.size() + EXTRANONCE_SIZE +
                                   EXTRANONCE2_SIZE);

    signature[0] = height_script.size();
    // 1 = size of height_script ^ it will never exceed 1 byte

    memcpy(signature.data() + 1, height_script.data(), height_script.size());
    memcpy(signature.data() + 1 + height_script.size(), coinbase_extra.data(),
           coinbase_extra.size());
    tx.AddInput(prevTxIn, UINT32_MAX, signature, UINT32_MAX);
    return sizeof(OutPoint) + signature.size() -
           (EXTRANONCE_SIZE + EXTRANONCE2_SIZE);
}

#endif