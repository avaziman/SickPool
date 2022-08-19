#ifndef BLOCK_TEMPLATE_HPP
#define BLOCK_TEMPLATE_HPP

#include <ctime>
#include <string_view>
#include <algorithm>
#include <vector>

#include "utils.hpp"
#include "hash_wrapper.hpp"
#include "static_config/static_config.hpp"

struct TransactionData
{
   public:
    // TransactionData(std::string_view data_hex) : data_hex(data_hex)
    // {
    //     data.resize(data_hex.size() / 2);
    //     Unhexlify(data.data(), data_hex.data(), data_hex.size());

    //     HashWrapper::SHA256d(hash, data.data(), data.size());
    // }

    std::string_view data_hex;
    std::vector<uint8_t> data;
    uint8_t hash[HASH_SIZE];
    double fee;
    
    TransactionData(std::string_view data_hex, std::string_view hash_hex)
        : data_hex(data_hex)
    {
        data.resize(data_hex.size() / 2);
        Unhexlify(data.data(), data_hex.data(), data_hex.size());

        Unhexlify(hash, hash_hex.data(), hash_hex.size());
        std::reverse(hash, hash + sizeof(hash));
    }

    // for test only
    TransactionData(std::string hashs){
        Unhexlify(hash, hashs.data(), hashs.size());
        std::reverse(hash, hash + sizeof(hash));
    }

    TransactionData() = default;
};

struct TransactionDataList
{
    std::vector<TransactionData> transactions;
    std::size_t byteCount = 0;

    TransactionDataList() { 
        // null td to be replaced by coinbase
        TransactionData td;
        memset(&td, 0, sizeof(td));
        transactions.push_back(td);
    }

    void AddCoinbaseTxData(const TransactionData& td)
    {
        std::size_t txSize = td.data.size();
        byteCount += txSize;

        // if there is no space for coinbase transaction, remove other txs
        while (byteCount + BLOCK_HEADER_SIZE + 2 > MAX_BLOCK_SIZE)
        {
            byteCount -= transactions.back().data.size();
            transactions.pop_back();
        }

        transactions[0] = td;
    }

    bool AddTxData(const TransactionData& td)
    {
        std::size_t txSize = td.data.size();

        // 2 bytes for tx count (max 65k)
        if (byteCount + txSize + BLOCK_HEADER_SIZE + 2 > MAX_BLOCK_SIZE)
            return false;

        // keep space for coinbase
        transactions.insert(transactions.begin() + 1, td);
        byteCount += txSize;

        return true;
    }
};

// in the order they appear in the rpc response
struct BlockTemplateSin
{
    int32_t version;
    std::string_view prevBlockHash;
    TransactionDataList txList;
    int64_t coinbaseValue;
    std::string_view target;
    int64_t minTime;
    std::string_view bits;
    uint32_t height;
};

// in the order they appear in the rpc response
struct BlockTemplate
{
    BlockTemplate() : txList() {}
    int32_t version;
    std::string_view prevBlockHash;
    std::string_view finalsRootHash;
    std::string_view solution;
    TransactionDataList txList;
    std::string_view coinbase_hex;
    int64_t coinbaseValue;
    std::string_view target;
    int64_t minTime;
    std::string_view bits;
    uint32_t height;
};

#endif