#include "job_manager.hpp"
#include "stratum_server.hpp"

Job* JobManager::GetNewJob()
{
    std::vector<char> resBody;
    int resCode =
        StratumServer::SendRpcReq(resBody, 1, "getblocktemplate", nullptr, 0);

    if (resCode != 200)
    {
        Logger::Log(
            LogType::Critical, LogField::JobManager,
            "Failed to get block template, http code: %d, response: %*.s",
            resCode, resBody.size(), resBody.data());
        return nullptr;
    }

    try
    {
        blockTemplate = ParseBlockTemplateJson(resBody);
    }
    catch (const simdjson_error& err)
    {
        Logger::Log(LogType::Critical, LogField::JobManager,
                    "Failed to parse block template: %s", err.what());
        return nullptr;
    }

    // reverse all fields to header encoding
    char prevBlockRev[64];
    char finalSRootRev[64];
    char bitsRev[8];
    // char sol[SOLUTION_SIZE * 2];

    ReverseHex(prevBlockRev, blockTemplate.prevBlockHash.data(), 64);
    ReverseHex(finalSRootRev, blockTemplate.finalSaplingRootHash.data(), 64);
    ReverseHex(bitsRev, blockTemplate.bits.data(), 8);
    // memcpy(sol144, solution.data(), SOLUTION_SIZE * 2);
    // memcpy(sol144, solution, 144);

    uint32_t versionRev = bswap_32(blockTemplate.version);
    uint32_t mintimeRev = bswap_32(blockTemplate.minTime);

    return new VerusJob(1, blockTemplate);
}

BlockTemplate JobManager::ParseBlockTemplateJson(std::vector<char>& json)
{
    BlockTemplate blockRes;
    ondemand::document doc = jsonParser.iterate(
        json.data(), json.size() - SIMDJSON_PADDING, json.size());

    ondemand::object res = doc["result"].get_object();

    // this must be in the order they appear in the result for simdjson
    blockRes.version = res["version"].get_int64();
    blockRes.prevBlockHash = res["previousblockhash"].get_string();
    blockRes.finalSaplingRootHash = res["finalsaplingroothash"].get_string();
    blockRes.solution = res["solution"].get_string();
    // can't iterate after we get the string_view
    ondemand::array txs = res["transactions"].get_array(); 

    for (auto tx : txs)
    {
        TransactionData td;
        std::string_view txDataHex = tx["data"].get_string();
        std::string_view txHashHex = tx["hash"].get_string();
        td.dataHex = txDataHex;
        td.fee = tx["fee"].get_double();

        int txSize = txDataHex.size() / 2;
        td.data = std::vector<unsigned char>(txSize);
        Unhexlify(td.data.data(), txDataHex.data(), txDataHex.size());
        Unhexlify(td.hash, txHashHex.data(), txHashHex.size());

        if (!blockRes.txList.AddTxData(td))
        {
            Logger::Log(LogType::Warn, LogField::JobManager,
                        "Block template is full! block size is %d bytes",
                        blockRes.txList.byteCount);
            break;
        }
    }
    
    blockRes.coinbaseValue = res["coinbasetxn"]["coinbasevalue"].get_int64();

    blockRes.minTime = res["mintime"].get_int64();
    blockRes.bits = res["bits"].get_string();
    blockRes.height = res["height"].get_int64();

    TransactionData coinbaseTx =
        GetCoinbaseTxData(blockRes.coinbaseValue, blockRes.height);
    
    // we need to hexlify here otherwise hex will be garbagep
    char coinbaseHex[coinbaseTx.data.size() * 2];
    Hexlify(coinbaseHex, coinbaseTx.data.data(), coinbaseTx.data.size());
    coinbaseTx.dataHex = std::string_view(coinbaseHex, sizeof(coinbaseHex));
    blockRes.txList.AddCoinbaseTxData(coinbaseTx);

    return blockRes;
}


// doesnt include dataHex
TransactionData JobManager::GetCoinbaseTxData(int64_t value, uint32_t height)
{
    TransactionData res;
    VerusTransaction coinbaseTx = GetCoinbaseTx(value, height);

    res.data = coinbaseTx.GetBytes();
    HashWrapper::VerushashV2b2(res.hash, res.data.data(), res.data.size());
    res.fee = 0;
    return res;
}

VerusTransaction JobManager::GetCoinbaseTx(int64_t value, uint32_t height)
{
    unsigned char prevTxIn[32] = {0};  // null last input
    unsigned int locktime = 0;         // null locktime (no locktime)

    VerusTransaction coinbaseTx =
        VerusTransaction(TXVERSION, locktime, true, TXVERSION_GROUP);
    // add signature with height script and extra data

    std::vector<unsigned char> heightScript = GetNumScript(height);
    const unsigned char heightScriptLen = heightScript.size();

    std::vector<unsigned char> signature(heightScriptLen +
                                         coinbaseExtra.size());
    signature[0] = heightScriptLen;
    memcpy(signature.data() + 1, heightScript.data(), heightScriptLen);
    memcpy(signature.data() + 1 + heightScriptLen, coinbaseExtra.data(),
           coinbaseExtra.size());

    coinbaseTx.AddInput(prevTxIn, UINT32_MAX, signature, UINT32_MAX);
    coinbaseTx.AddP2PKHOutput(StratumServer::coin_config.pool_addr, value);
    return coinbaseTx;
}