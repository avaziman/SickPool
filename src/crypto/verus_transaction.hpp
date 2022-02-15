#ifndef VERUS_TRANSACTION_HPP_
#define VERUS_TRANSACTION_HPP_
#include <iomanip>
#include <iostream>
#include <tuple>
#include <vector>

#include "transaction.hpp"
#include "utils.hpp"
#include "verushash/endian.h"

class VerusTransaction : public Transaction
{
   public:
    VerusTransaction(int32_t ver, uint32_t locktime, bool overwintered, std::string verGroupId)
        : Transaction(ver, locktime),
          is_overwintered(overwintered),
          version_groupid(verGroupId)
    {
    }

    std::string GetHex()
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8)
           << htobe32(this->version | (is_overwintered << 31));
        ss << ReverseHex(version_groupid);

        ss << VarInt(vin.size());
        for (Input input : this->vin)
        {
            ss << input.previous_output.hash;
            ss << std::hex << std::setfill('0') << std::setw(8)
               << htobe32(input.previous_output.index);

            ss << VarInt(input.signature_script.size() / 2);
            ss << input.signature_script;
            
            ss << std::hex << std::setfill('0') << std::setw(8)
               << htobe32(input.sequence);
        }

        ss << VarInt(vout.size());
        for (Output output : this->vout)
        {
            ss << std::hex << std::setfill('0') << std::setw(16)
               << htobe64(output.value);
            ss << VarInt(output.pk_script.size() / 2);
            ss << output.pk_script;
        }

        ss << std::hex << std::setfill('0') << std::setw(8)
           << htobe32(this->lock_time);
        ss << std::hex << std::setfill('0') << std::setw(8)
           << htobe32(this->expiryHeight);
        ss << "0000000000000000000000";

        return ss.str();
    }

    void AddTestnetCoinbaseOutput(){
       Output output;
       output.value = 0;
       output.pk_script =
           "1a040300010114a23f82866c21819f55a1668ba7b9932e6d326b1ecc32040314010"
           "114a23f82866c21819f55a1668ba7b9932e6d326b1e1701a6ef9ea235635e328124"
           "ff3429db9f9e91b64e2d000175";
       vout.push_back(output);
    }

   private:
    const bool is_overwintered;
    std::string version_groupid;
    const uint32_t expiryHeight = 0;
};
#endif