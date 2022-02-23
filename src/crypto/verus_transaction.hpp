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
    VerusTransaction(int32_t ver, uint32_t locktime, bool overwintered, uint32_t verGroupId)
        : Transaction(ver, locktime),
          is_overwintered(overwintered),
          version_groupid(verGroupId)
    {
    }

    std::vector<unsigned char>* GetBytes() override
    {
        int ver = version | (is_overwintered << 31);
        WriteData(&ver, 4);

        uint64_t varIntVal = vin.size();
        int varIntLen = VarInt(varIntVal);
        WriteData(&varIntVal, varIntLen);

        for (Input input : this->vin)
        {
            WriteData(input.previous_output.hash, 32);
            WriteData(&input.previous_output.index, 4);

            varIntVal = input.signature_script.size();
            varIntLen = VarInt(varIntVal);

            WriteData(&varIntVal, varIntLen);
            WriteData(&input.signature_script, input.signature_script.size());

            WriteData(&input.sequence, 4);
        }

        varIntVal = vout.size();
        varIntLen = VarInt(varIntVal);

        WriteData(&varIntVal, varIntLen);
        for (Output output : this->vout)
        {
            WriteData(&output.value, 8);
            
            varIntVal = output.pk_script.size();
            varIntLen = VarInt(varIntVal);

            WriteData(&varIntVal, varIntLen);
            WriteData(&output.pk_script, output.pk_script.size());
        }

        WriteData(&lock_time, 4);
        WriteData(&expiryHeight, 4);

        memset(bytes.data() + written, '0', 11);

        return &bytes;
    }


    void AddTestnetCoinbaseOutput(){
       Output output;
       output.value = 0;
    //    output.pk_script =
    //        "1a040300010114a23f82866c21819f55a1668ba7b9932e6d326b1ecc32040314010"
    //        "114a23f82866c21819f55a1668ba7b9932e6d326b1e1701a6ef9ea235635e328124"
    //        "ff3429db9f9e91b64e2d000175";
       vout.push_back(output);
    }

   private:
   template <typename T>
    void WriteData(T *val, int size) {
       memcpy(bytes.data() + written, val, size);
       written += size;
    }

    int written = 0;
    const bool is_overwintered;
    const uint32_t version_groupid;
    const uint32_t expiryHeight = 0;
};
#endif