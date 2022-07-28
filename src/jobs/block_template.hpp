#ifndef BLOCK_TEMPLATE_HPP
#define BLOCK_TEMPLATE_HPP

#include <ctime>
#include <string_view>
#include <vector>
#include "static_config/config.hpp"

struct TransactionData
{
    std::string_view dataHex;
    std::vector<unsigned char> data;
    unsigned char hash[32];
    double fee;
};

struct TransactionDataList{
    std::vector<TransactionData> transactions{1};
    std::size_t byteCount = 0;

    TransactionDataList() = default;

    void AddCoinbaseTxData(const TransactionData& td) { 
        std::size_t txSize = td.data.size();
        byteCount += txSize;

        // if there is no space for coinbase transaction, remove other txs
        while (byteCount + BLOCK_HEADER_SIZE + 2 > MAX_BLOCK_SIZE){
            byteCount -= transactions.back().data.size();
            transactions.pop_back();
        }

        transactions[0] = td;
    }

    bool AddTxData(const TransactionData& td)
    {
        std::size_t txSize = td.data.size();

        // 2 bytes for tx count (max 65k)
        if (byteCount + txSize + BLOCK_HEADER_SIZE + 2 > MAX_BLOCK_SIZE) return false;

        // keep space for coinbase
        transactions.insert(transactions.begin() + 1, td);
        byteCount += txSize;

        return true;
    }
};

// in the order they appear in the rpc response
struct BlockTemplate
{
    BlockTemplate() : txList(){}
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