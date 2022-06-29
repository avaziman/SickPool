#ifndef VERUS_JOB_HPP_
#define VERUS_JOB_HPP_
#include <experimental/array>
#include <iomanip>
#include <sstream>
#include <string>
#include <byteswap.h>

#include "job.hpp"

// #define EXTRANONCE_SIZE 4
// #define VERSION_SIZE 4
// #define TIME_SIZE 4
// #define BITS_SIZE 4
// #define PREVHASH_SIZE HASH_SIZE
// #define MERKLE_ROOT_SIZE HASH_SIZE
// #define FINALSROOT_SIZE HASH_SIZE
// #define NONCE_SIZE HASH_SIZE
// #define SOLUTION_SIZE 1344
// #define SOLUTION_LENGTH_SIZE 3

// #define BLOCK_HEADER_STATIC_SIZE                                  \
//     VERSION_SIZE           /* version */                          \
//         + PREVHASH_SIZE    /* prevhash */                         \
//         + MERKLE_ROOT_SIZE /* merkle_root */                      \
//         + FINALSROOT_SIZE  /* final sapling root */               \
//         + TIME_SIZE        /* time, not static but we override */ \
//         + BITS_SIZE        /* bits */
class VerusJob : public Job
{
   public:
    VerusJob(uint32_t jobId, const BlockTemplate& bTemplate, bool clean = true)
        : Job(jobId, bTemplate)
    {
        char merkleRootHex[MERKLE_ROOT_SIZE * 2];

        // difficulty is calculated from opposite byte encoding than in block
        uint32_t bitsUint = HexToUint(bTemplate.bits.data(), 8);
        this->targetDiff = BitsToDiff(bitsUint);
        this->expectedShares = GetExpectedHashes(this->targetDiff);


        // reverse all numbers for block encoding

        char prevBlockRevHex[PREVHASH_SIZE * 2];
        char finalSRootRevHex[FINALSROOT_SIZE * 2];

        ReverseHex(prevBlockRevHex, bTemplate.prevBlockHash.data(), PREVHASH_SIZE * 2);
        ReverseHex(finalSRootRevHex, bTemplate.finalsRootHash.data(),
                   FINALSROOT_SIZE * 2);

        // write header

        Write(&bTemplate.version, VERSION_SIZE);  // no need to reverse here
        WriteUnhex(prevBlockRevHex, PREVHASH_SIZE * 2);

        // hashes are given in LE, no need to reverse

        MerkleTree::CalcRoot(bTemplate.txList.transactions,
                             staticHeaderData + written);

        // we need the hexlified merkle root for the notification message
        Hexlify(merkleRootHex, staticHeaderData + written, MERKLE_ROOT_SIZE);
        written += MERKLE_ROOT_SIZE;

        WriteUnhex(finalSRootRevHex, FINALSROOT_SIZE * 2);

        Write(&bTemplate.minTime, TIME_SIZE);  // we overwrite time later
        Write(&bitsUint, BITS_SIZE);

        // only write the sol size and 72 first bytes of sol
        // the rest is changed by the miner
        // unsigned char solLen[] = {0xfd, 0x40, 0x05};
        // written += 32; // skip nonce

        // WriteUnhexlify(bTemplate.solution, 144);

        // only generating notify message once for efficiency
        auto start = std::chrono::steady_clock::now();

        // reverse all numbers for notify, they are written in correct order
        int32_t revVer = bswap_32(bTemplate.version);
        int64_t revMinTime = bswap_32(bTemplate.minTime);
        bitsUint = bswap_32(bitsUint);

        notifyBuffSize =
            snprintf(notifyBuff, MAX_NOTIFY_MESSAGE_SIZE,
                     "{\"id\":null,\"method\":\"mining.notify\",\"params\":"
                     "[\"%.8s\",\"%08x\",\"%.64s\",\"%.64s\",\"%.64s\",\"%"
                     "08x\",\"%08x\",%s,\"%.144s\"]}\n",
                     GetId().data(), revVer, prevBlockRevHex, merkleRootHex,
                     finalSRootRevHex, (uint32_t)revMinTime, bitsUint,
                     BoolToCstring(clean), bTemplate.solution.data());

        auto end = std::chrono::steady_clock::now();

        std::cout << "sprintf dur: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(
                         end - start)
                         .count()
                  << "us" << std::endl;

        std::cout << notifyBuff << std::endl;
    }

    void GetHeaderData(uint8_t* buff, std::string_view time,
                           std::string_view nonce1, std::string_view nonce2,
                           std::string_view sol) const/* override */
    {
        memcpy(buff, this->staticHeaderData, BLOCK_HEADER_STATIC_SIZE);

        // time pos
        int pos =
            VERSION_SIZE + PREVHASH_SIZE + MERKLE_ROOT_SIZE + FINALSROOT_SIZE;

        Unhexlify(buff + pos, time.data(), TIME_SIZE * 2);

        // nonce1 pos
        pos += TIME_SIZE + BITS_SIZE;  // bits are already written from static

        Unhexlify(buff + pos, nonce1.data(), EXTRANONCE_SIZE * 2);

        // nonce2 pos
        pos += EXTRANONCE_SIZE;
        Unhexlify(buff + pos, nonce2.data(),
                  (NONCE_SIZE - EXTRANONCE_SIZE) * 2);

        // solution pos
        pos += (NONCE_SIZE - EXTRANONCE_SIZE);
        Unhexlify(buff + pos, sol.data(),
                  (SOLUTION_LENGTH_SIZE + SOLUTION_SIZE) * 2);
    }

   private:
    // uint8_t staticHeaderData[BLOCK_HEADER_STATIC_SIZE];
    inline void WriteUnhex(const char* data, int size)
    {
        Unhexlify(staticHeaderData + written, data, size);
        written += size / 2;
    }

    template <class T>
    inline void Write(T* data, int size)
    {
        memcpy(staticHeaderData + written, data, size);
        written += size;
    }

    // char* GetVersion() { return version; }
    // char* GetPrevBlockhash() { return hashPrevBlock; }
    // char* GetTime() { return nTime; }
    // char* GetBits() { return nBits; }

   private:
    int written = 0;
};

using job_t = VerusJob;

#endif