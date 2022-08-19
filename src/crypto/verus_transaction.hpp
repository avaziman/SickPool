#ifndef VERUS_TRANSACTION_HPP_
#define VERUS_TRANSACTION_HPP_
#include <iomanip>
#include <iostream>
#include <tuple>
#include <vector>

#include "static_config.hpp"
#include "transaction.hpp"
#include "utils.hpp"
#include "verushash/endian.h"

class VerusTransaction : public Transaction
{
   public:
    VerusTransaction(int32_t ver, uint32_t locktime, bool overwintered,
                     uint32_t verGroupId)
        : Transaction(ver, locktime),
          is_overwintered(overwintered),
          version_groupid(verGroupId)
    {
    }

    void GetBytes(std::vector<unsigned char>& bytes) override;

    // since PBAAS_ACTIVATE
    void AddFeePoolOutput(std::string_view coinbaseHex);

   private:
    template <typename T>
    inline void WriteData(unsigned char* bytes, T* val, int size)
    {
        memcpy(bytes + written, val, size);
        written += size;
    }

    int written = 0;
    const bool is_overwintered;
    const uint32_t version_groupid;
    const uint32_t expiryHeight = 0;
};
#endif