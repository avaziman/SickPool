#include "job_manager.hpp"

const job_t* JobManager::GetNewJob()
{
    using namespace simdjson;
    std::string json;
    int resCode = daemon_manager->SendRpcReq<>(json, 1, "getblocktemplate");

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

// doesnt include dataHex
TransactionData JobManager::GetCoinbaseTxData(int64_t value, uint32_t height,
                                              int64_t locktime,
                                              std::string_view rpc_coinbase)
{
    TransactionData res;
    VerusTransaction coinbaseTx =
        GetCoinbaseTx(value, height, locktime, rpc_coinbase);

    coinbaseTx.GetBytes(res.data);
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