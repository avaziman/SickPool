#ifndef VERUS_JOB
#define VERUS_JOB

#include "../crypto/verus_header.hpp"
#include "../crypto/verushash/uint256.h"
#include "job.hpp"

class VerusJob : public Job
{
   public:
    VerusJob(uint32_t id, Block block) : Job(id, block) {}
    int GetNotifyMessage(char* message, size_t size)
    {
        VerusBlockHeader* verusHeader = (VerusBlockHeader*)this->Header();

        int len = snprintf(
            message, size,
            "{\"id\": null, \"method\": \"mining.notify\", \"params\": "
            "[\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "true, \"%s\"]}\n",
            this->GetId().c_str(),                       // JOB_ID = extraNonce
            verusHeader->GetVersion(),           // VERSION
            verusHeader->GetPrevBlockhash(),     // PREVHASH
            this->GetMerkleRoot(),               // MERKLEROOT
            verusHeader->GetFinalSaplingRoot(),  // RESERVED
            verusHeader->GetTime(),              // TIME
            verusHeader->GetBits(),              // BITS
            verusHeader->GetSol144());  // first 72 bytes of solution

        std::cout << "job notify -> " << message << std::endl;
        return len;
    }

   private:
};

#endif