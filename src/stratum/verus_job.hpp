#ifndef VERUS_JOB_HPP_
#define VERUS_JOB_HPP_
#include <iomanip>
#include <sstream>
#include <string>

#include "job.hpp"

class VerusJob : public Job
{
   public:
    VerusJob(uint32_t jobId, std::vector<std::vector<unsigned char>>& txs,
             bool clean, const char* ver, const char* prevBlock,
             const char* time, const char* bits, const char* finalSaplingRoot,
             const char* sol)
        : Job(jobId, txs)
    {
        uint32_t bitsUint = bswap_32(FromHex(bits));
        this->targetDiff = BitsToDiff(bitsUint);

        char merkleRootHex[65];
        char sol144[145];
        // cstring for sprintf
        merkleRootHex[64] = sol144[144] = 0;

        memcpy(sol144, sol, 144);

        // we first need to copy the values because we can't modify cstring
        // then we can unhexlify them
        WriteHex(ver, 8);
        WriteHex(prevBlock, 64);

        MerkleTree::CalcRoot(txs, headerData + written);
        // we need the hexlified merkle root for the notification
        Hexlify(headerData + written, 32, merkleRootHex);
        written += 32;

        WriteHex(finalSaplingRoot, 64);

        WriteHex(time, 8);    // we overwrite time later
        Write(&bitsUint, 4);  // use the uint for the correct byte order

        // only write the sol size and 72 first bytes of sol
        // the rest is changed by the miner
        // unsigned char solLen[] = {0xfd, 0x40, 0x05};
        // written += 32; // skip nonce

        // WriteUnhexlify(sol144, 144);

        // only generating notify message once for efficiency

        notifyBuffSize = snprintf(
            notifyBuff, MAX_NOTIFY_MESSAGE_SIZE,
            "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"%s\","
            "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%s,\"%s\"]}\n",
            GetId(), ver, prevBlock, merkleRootHex, finalSaplingRoot, time, bits,
            BoolToCstring(clean), sol144);
        std::cout << notifyBuff << std::endl;
    }

    unsigned char* GetHeaderData(const char* time, const char* nonce1,
                           const char* nonce2, const char* sol,
                           int solSize) override
    {
        Unhexlify((char*)time, 8, this->headerData + 4 + 32 * 3);
        Unhexlify((char*)nonce1, 8, this->headerData + 4 * 3 + 32 * 3);
        Unhexlify((char*)nonce2, 64 - 8, this->headerData + 4 + 4 * 3 + 32 * 3);
        Unhexlify((char*)sol, solSize,
                  this->headerData + 4 * 3 + 32 * 4);


        return this->headerData;
    }

   private:
    inline void WriteHex(const char* data, int size)
    {
        Unhexlify((char*)data, size, headerData + written);
        written += size / 2;
    }

    template <class T>
    inline void Write(T* data, int size)
    {
        memcpy(headerData + written, data, size);
        written += size;
    }

    // char* GetVersion() { return version; }
    // char* GetPrevBlockhash() { return hashPrevBlock; }
    // char* GetTime() { return nTime; }
    // char* GetBits() { return nBits; }

   protected:
    int written = 0;
};
#endif