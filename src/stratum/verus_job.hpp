#ifndef VERUS_JOB_HPP_
#define VERUS_JOB_HPP_
#include <experimental/array>
#include <iomanip>
#include <sstream>
#include <string>

#include "job.hpp"
class VerusJob : public Job
{
   public:
    VerusJob(uint32_t jobId, BlockTemplate& bTemplate, bool clean = true)
        : Job(jobId, bTemplate.coinbaseValue, bTemplate.height, bTemplate.txList,
              bTemplate.minTime)
    {
        char merkleRootHex[64];

        // difficulty is calculated from opposite byte encoding than in block
        uint32_t bitsUint = HexToUint(bTemplate.bits.data(), 8);
        this->targetDiff = BitsToDiff(bitsUint);

        // reverse all numbers for block encoding

        char prevBlockRev[64];
        char finalSRootRev[64];

        ReverseHex(prevBlockRev, bTemplate.prevBlockHash.data(), 64);
        ReverseHex(finalSRootRev, bTemplate.finalSaplingRootHash.data(), 64);

        // write header

        Write(&bTemplate.version, 4);  // no need to reverse here
        WriteUnhex(prevBlockRev, 64);

        // hashes are given in LE, no need to reverse

        MerkleTree::CalcRoot(bTemplate.txList.transactions,
                            headerData + written);

        // we need the hexlified merkle root for the notification message
        Hexlify(merkleRootHex, headerData + written, 32);
        written += 32;

        WriteUnhex(finalSRootRev, 64);

        Write(&bTemplate.minTime, 4);  // we overwrite time later
        Write(&bitsUint, 4);

        // only write the sol size and 72 first bytes of sol
        // the rest is changed by the miner
        // unsigned char solLen[] = {0xfd, 0x40, 0x05};
        // written += 32; // skip nonce

        // WriteUnhexlify(bTemplate.solution, 144);

        // only generating notify message once for efficiency
        auto start = std::chrono::steady_clock::now();

        // reverse all numbers for notify, they are written in correct order
        bTemplate.version = bswap_32(bTemplate.version);
        bTemplate.minTime = bswap_32(bTemplate.minTime);
        bitsUint = bswap_32(bitsUint);
        
        notifyBuffSize =
            snprintf(notifyBuff, MAX_NOTIFY_MESSAGE_SIZE,
                     "{\"id\":null,\"method\":\"mining.notify\",\"params\":"
                     "[\"%.8s\",\"%08x\",\"%.64s\",\"%.64s\",\"%.64s\",\"%"
                     "08x\",\"%08x\",%s,\"%.144s\"]}\n",
                     GetId(), bTemplate.version, prevBlockRev, merkleRootHex,
                     finalSRootRev, (uint32_t)bTemplate.minTime, bitsUint,
                     BoolToCstring(clean), bTemplate.solution.data());

        auto end = std::chrono::steady_clock::now();

        std::cout << "sprintf dur: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(
                         end - start)
                         .count()
                  << "us" << std::endl;

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