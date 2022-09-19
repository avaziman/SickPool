#ifndef TRANSACTION_BTC
#define TRANSACTION_BTC

#include <string_view>
#include <utility>
#include <vector>

#include "transaction.hpp"

class TransactionBtc : public Transaction
{
   public:
    using Transaction::Transaction;

    std::size_t GetBytes(std::vector<uint8_t>& bytes) /*override*/;
    TransactionBtc GetCoinbase(int64_t value, uint32_t height);
};

using transaction_t = TransactionBtc;

#endif