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
    VerusJob(uint32_t jobId, BlockTemplate bTemplate)
        : Job(jobId, bTemplate.coinbaseValue, bTemplate.txList,
              bTemplate.minTime)
    {
        char merkleRootHex[64];

        // difficulty is calculated from opposite byte encoding than in block
        uint32_t bitsUint = HexToUint(bTemplate.bits.data(), 8);
        this->targetDiff = BitsToDiff(bitsUint);

        // reverse all variables to block encoding

        Write(&bTemplate.version, 4);
        WriteUnhex(bTemplate.prevBlockHash.data(), 64, true);

        std::vector<std::array<unsigned char, 32>> hashes(
            bTemplate.txList.transactions.size());
        for (int i = 0; i < hashes.size(); i++)
            hashes[i] = std::experimental::to_array(bTemplate.txList.transactions[i].hash);

        MerkleTree::CalcRoot(hashes, headerData + written);
        written += 32;
        // we need the hexlified merkle root for the notification message
        Hexlify(merkleRootHex, headerData + written, 32);

        WriteUnhex(bTemplate.finalSaplingRootHash.data(), 64, true);

        Write(&bTemplate.minTime, 4);      // we overwrite time later
        Write(&bitsUint, 4);  // use the uint for the correct byte order

        // // only write the sol size and 72 first bytes of sol
        // // the rest is changed by the miner
        // // unsigned char solLen[] = {0xfd, 0x40, 0x05};
        // // written += 32; // skip nonce

        // // WriteUnhexlify(sol144, 144);

        // // only generating notify message once for efficiency
        // auto start = std::chrono::steady_clock::now();

        // notifyBuffSize = snprintf(
        //     notifyBuff, MAX_NOTIFY_MESSAGE_SIZE,
        //     "{\"id\":null,\"method\":\"mining.notify\",\"params\":"
        //     "[\"%.8s\",\"%08x\",\"%.64s\",\"%.64s\",\"%.64s\",\"%08x\",\"%."
        //     "8s\",%s,"
        //     "\"%.144s\"]}\n",
        //     GetId(), bswap_32(ver), prevBlock, merkleRootHex,
        //     finalSaplingRoot, bswap_32(time), bits, BoolToCstring(clean),
        //     solution);

        // auto end = std::chrono::steady_clock::now();

        // std::cout << "sprintf dur: "
        //           << std::chrono::duration_cast<std::chrono::microseconds>(
        //                  end - start)
        //                  .count()
        //           << "us" << std::endl;
        // ;
        // std::cout << notifyBuff << std::endl;
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
    inline void WriteUnhex(const char* data, int size, bool reverse)
    {
        Unhexlify(headerData + written, data, size);
        if (reverse)
        {
            for (int i = 0; i < size; i++)
                headerData[written + i] = headerData[written + size - 1 - i];
        }
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