#include "job_manager.hpp"

const job_t* JobManager::GetNewJob()
{
    using namespace simdjson;
    std::string json;
    int resCode = daemon_manager->SendRpcReq<>(json, 1, "getblocktemplate");

    if (resCode != 200)
    {
        Logger::Log(
            LogType::Critical, LogField::JobManager,
            "Failed to get block template, http code: %d, response: %s",
            resCode, json.c_str());
        return nullptr;
    }

    return GetNewJob(json);
}

const job_t* JobManager::GetNewJob(const std::string& json_template)
{
    using namespace simdjson;
    try
    {
        ondemand::document doc =
            jsonParser.iterate(json_template.data(), json_template.size(),
                               json_template.capacity());

        blockTemplate = BlockTemplate();
        ondemand::object res = doc["result"].get_object();

        // this must be in the order they appear in the result for simdjson
        blockTemplate.version =
            static_cast<uint32_t>(res["version"].get_int64());
        blockTemplate.prevBlockHash = res["previousblockhash"].get_string();
        blockTemplate.finalsRootHash = res["finalsaplingroothash"].get_string();
        blockTemplate.solution = res["solution"].get_string();
        // can't iterate after we get the string_view
        ondemand::array txs = res["transactions"].get_array();

        // for (auto tx : txs)
        // {
        //     TransactionData td;
        //     std::string_view txHashHex;
        //     td.dataHex = tx["data"].get_string();
        //     txHashHex = tx["hash"].get_string();
        //     td.fee = tx["fee"].get_double();

        //     std::cout << "tx data: " << td.dataHex << std::endl;

        //     int txSize = td.dataHex.size() / 2;
        //     td.data = std::vector<unsigned char>(txSize);
        //     Unhexlify(td.data.data(), td.dataHex.data(), td.dataHex.size());
        //     Unhexlify(td.hash, txHashHex.data(), txHashHex.size()); // hash
        //     is reversed std::reverse(td.hash, td.hash + 32);

        //     if (!blockTemplate.txList.AddTxData(td))
        //     {
        //         Logger::Log(LogType::Warn, LogField::JobManager,
        //                     "Block template is full! block size is %d bytes",
        //                     blockTemplate.txList.byteCount);
        //         break;
        //     }
        // }

        blockTemplate.coinbase_hex = res["coinbasetxn"]["data"].get_string();
        blockTemplate.coinbaseValue =
            res["coinbasetxn"]["coinbasevalue"].get_int64();

        blockTemplate.target = res["target"].get_string();
        blockTemplate.minTime = res["mintime"].get_int64();
        blockTemplate.bits = res["bits"].get_string();
        blockTemplate.height = static_cast<uint32_t>(res["height"].get_int64());

        TransactionData coinbaseTx = GetCoinbaseTxData(
            blockTemplate.coinbaseValue, blockTemplate.height,
            blockTemplate.minTime, blockTemplate.coinbase_hex);

        // we need to hexlify here otherwise hex will be garbage
        char coinbaseHex[coinbaseTx.data.size() * 2];
        Hexlify(coinbaseHex, coinbaseTx.data.data(), coinbaseTx.data.size());
        coinbaseTx.dataHex = std::string_view(coinbaseHex, sizeof(coinbaseHex));
        blockTemplate.txList.AddCoinbaseTxData(coinbaseTx);

        std::string jobIdHex(8, '0');
        ToHex(jobIdHex.data(), job_count);

        job_t job(jobIdHex, blockTemplate);

        last_job_id_hex = jobIdHex;
        job_count++;

        return &jobs.emplace_back(std::move(job));
    }
    catch (const simdjson::simdjson_error& err)
    {
        Logger::Log(LogType::Critical, LogField::JobManager,
                    "Failed to parse block template: %s, json: %s", err.what(),
                    json_template.c_str());
    }
    return nullptr;
}

// doesnt include dataHex
TransactionData JobManager::GetCoinbaseTxData(int64_t value, uint32_t height,
                                              int64_t locktime,
                                              std::string_view rpc_coinbase)
{
    TransactionData res;
    VerusTransaction coinbaseTx =
        GetCoinbaseTx(value, height, locktime, rpc_coinbase);

    res.data = coinbaseTx.GetBytes();
    HashWrapper::SHA256d(res.hash, res.data.data(), res.data.size());
    res.fee = 0;
    return res;
}

VerusTransaction JobManager::GetCoinbaseTx(int64_t value, uint32_t height,
                                           int64_t locktime,
                                           std::string_view rpc_coinbase)
{
    unsigned char prevTxIn[32] = {0};  // null last input
    // unsigned int locktime = 0;         // null locktime (no locktime)

    VerusTransaction coinbaseTx(TXVERSION, locktime, true, TXVERSION_GROUP);
    // add signature with height script and extra data

    std::vector<unsigned char> heightScript = GenNumScript(height);
    const unsigned char heightScriptLen = heightScript.size();

    std::vector<unsigned char> signature(1 + heightScriptLen +
                                         coinbaseExtra.size());
    signature[0] = heightScriptLen;
    // 1 = size of heightScript ^ it will never exceed 1 byte
    memcpy(signature.data() + 1, heightScript.data(), heightScriptLen);
    memcpy(signature.data() + 1 + heightScriptLen, coinbaseExtra.data(),
           coinbaseExtra.size());

    coinbaseTx.AddInput(prevTxIn, UINT32_MAX, signature, UINT32_MAX);
    coinbaseTx.AddP2PKHOutput(pool_addr, value);
#if POOL_COIN == COIN_VRSCTEST
    coinbaseTx.AddFeePoolOutput(
        rpc_coinbase);  // without this gives bad-blk-fees
#endif
    return coinbaseTx;
}