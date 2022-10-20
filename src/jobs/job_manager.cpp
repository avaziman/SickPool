#include "job_manager.hpp"

// const job_t* JobManager::GetNewJob()
// {
//     using namespace simdjson;
//     std::string json;
//     int resCode = daemon_manager->SendRpcReq(json, 1, "getblocktemplate");

//     if (resCode != 200)
//     {
//         logger.Log<LogType::Critical>( 
//                     "Failed to get block template, http code: {}, response: {}",
//                     strerror(resCode), json);
//         // TODO: make sock err negative maybe http positive to diffrinciate
//         return nullptr;
//     }

//     return GetNewJob(json);
// }

transaction_t JobManager::GetCoinbaseTx(int64_t value, uint32_t height,
                                        std::string_view rpc_coinbase)
{
    unsigned char prevTxIn[HASH_SIZE] = {0};  // null last input
    // unsigned int locktime = 0;         // null locktime (no locktime)

    transaction_t coinbaseTx;
    // add signature with height script and extra data

    std::vector<uint8_t> heightScript = GenNumScript(height);
    const unsigned char heightScriptLen = heightScript.size();

    
    std::vector<unsigned char> signature(1 + heightScriptLen +
                                         coinbase_extra.size());
    signature[0] = heightScriptLen;
    // 1 = size of heightScript ^ it will never exceed 1 byte
    memcpy(signature.data() + 1, heightScript.data(), heightScriptLen);
    memcpy(signature.data() + 1 + heightScriptLen, coinbase_extra.data(),
           coinbase_extra.size());

    coinbaseTx.AddInput(prevTxIn, UINT32_MAX, signature, UINT32_MAX);
    coinbaseTx.AddP2PKHOutput(pool_addr, value);

#if SICK_COIN == VRSCTEST
//TODO: do this as extra output
    coinbaseTx.AddFeePoolOutput(
        rpc_coinbase);  // without this gives bad-blk-fees
#endif
    return coinbaseTx;
}