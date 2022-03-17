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
             bool clean, uint32_t ver, const char* prevBlock, uint32_t time,
             const char* bits, const char* finalSaplingRoot, const char* sol144)
        : Job(jobId, txs, time)
    {
        /* we use cstring for "readable" generation of notify message
         and because we need to reverse (copy) the string_views anyway so might
         as well add a null char
        */
        char merkleRootHex[65];
        merkleRootHex[64] = '\0';

        uint32_t bitsUint = bswap_32(FromHex(bits));
        this->targetDiff = BitsToDiff(bitsUint);

        Write(&ver, 4);
        WriteUnhex(prevBlock, 64);

        MerkleTree::CalcRoot(txs, headerData + written);
        // we need the hexlified merkle root for the notification message
        Hexlify(merkleRootHex, headerData + written, 32);
        written += 32;

        WriteUnhex(finalSaplingRoot, 64);

        Write(&time, 4);      // we overwrite time later
        Write(&bitsUint, 4);  // use the uint for the correct byte order

        // only write the sol size and 72 first bytes of sol
        // the rest is changed by the miner
        // unsigned char solLen[] = {0xfd, 0x40, 0x05};
        // written += 32; // skip nonce

        // WriteUnhexlify(sol144, 144);

        // only generating notify message once for efficiency
        auto start = std::chrono::steady_clock::now();

        notifyBuffSize = snprintf(
            notifyBuff, MAX_NOTIFY_MESSAGE_SIZE,
            "{\"id\":null,\"method\":\"mining.notify\",\"params\":"
            "[\"%s\",\"%08x\",\"%s\",\"%s\",\"%s\",\"%08x\",\"%s\",%s,"
            "\"%s\"]}\n",
            GetId(), bswap_32(ver), prevBlock, merkleRootHex, finalSaplingRoot,
            bswap_32(time), bits, BoolToCstring(clean), sol144);

        auto end = std::chrono::steady_clock::now();

        std::cout << "sprintf dur: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
                  << "us" << std::endl;
        ;
        std::cout << notifyBuff << std::endl;
    }

    unsigned char* GetHeaderData(std::string_view time, std::string_view nonce1,
                                 std::string_view nonce2,
                                 std::string_view sol) override
    {
        // here we don't mutate the parameters so we use string_views as
        // received by the json parser
        Unhexlify(this->headerData + 4 + 32 * 3, time.data(), time.size());
        Unhexlify(this->headerData + 4 * 3 + 32 * 3, nonce1.data(),
                  nonce1.size());
        Unhexlify(this->headerData + 4 * 4 + 32 * 3, nonce2.data(),
                  64 - nonce1.size());
        Unhexlify(this->headerData + 4 * 3 + 32 * 4, sol.data(), sol.size());

        return this->headerData;
    }

   private:
    inline void WriteUnhex(const char* data, int size)
    {
        Unhexlify(headerData + written, data, size);
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