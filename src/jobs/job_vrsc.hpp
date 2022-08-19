#ifndef VERUS_JOB_HPP_
#define VERUS_JOB_HPP_
#include <byteswap.h>
#include <fmt/format.h>

#include <experimental/array>
#include <iomanip>
#include <sstream>
#include <string>

#include "block_template.hpp"
#include "job.hpp"
#include "static_config.hpp"

class VerusJob : public Job
{
   public:
    VerusJob(const std::string& jobId, const BlockTemplate& bTemplate,
             bool is_payment, bool clean = true)
        : Job(jobId, bTemplate, is_payment)
    {
        char merkleRootHex[MERKLE_ROOT_SIZE * 2];

        // difficulty is calculated from opposite byte encoding than in block
        uint32_t bitsUint = HexToUint(bTemplate.bits.data(), BITS_SIZE * 2);
        this->target_diff = BitsToDiff(bitsUint);
        this->expected_shares = GetExpectedHashes(this->target_diff);

        // reverse all numbers for block encoding

        char prevBlockRevHex[PREVHASH_SIZE * 2];
        char finalSRootRevHex[FINALSROOT_SIZE * 2];

        ReverseHex(prevBlockRevHex, bTemplate.prevBlockHash.data(),
                   PREVHASH_SIZE * 2);
        ReverseHex(finalSRootRevHex, bTemplate.finalsRootHash.data(),
                   FINALSROOT_SIZE * 2);

        // write header

        constexpr auto VERSION_OFFSET = 0;
        WriteStatic(VERSION_OFFSET, &bTemplate.version, VERSION_SIZE);

        constexpr auto PREVHASH_OFFSET = VERSION_OFFSET + VERSION_SIZE;
        WriteUnhexStatic(PREVHASH_OFFSET, prevBlockRevHex, PREVHASH_SIZE * 2);

        // hashes are given in LE, no need to reverse
        constexpr auto MROOT_OFFSET = PREVHASH_OFFSET + PREVHASH_SIZE;
        MerkleTree::CalcRoot(static_header_data + MROOT_OFFSET,
                             bTemplate.txList.transactions);

        constexpr auto FSROOT_OFFSET = MROOT_OFFSET + MERKLE_ROOT_SIZE;
        WriteUnhexStatic(FSROOT_OFFSET, finalSRootRevHex, FINALSROOT_SIZE * 2);

        constexpr auto TIME_OFFSET = FSROOT_OFFSET + FINALSROOT_SIZE;
        WriteStatic(TIME_OFFSET, &bTemplate.minTime,
              TIME_SIZE);  // we overwrite time later

        constexpr auto BITS_OFFSET = TIME_OFFSET + TIME_SIZE;
        WriteStatic(BITS_OFFSET, &bitsUint, BITS_SIZE);

        // we need the hexlified merkle root for the notification message
        Hexlify(merkleRootHex, static_header_data + MROOT_OFFSET,
                MERKLE_ROOT_SIZE);

        // only write the sol size and 72 first bytes of sol
        // the rest is changed by the miner
        // unsigned char solLen[] = {0xfd, 0x40, 0x05};
        // written += 32; // skip nonce

        // WriteWriteUnhex(lution, 144);

        // only generating notify message once for efficiency

        // reverse all numbers for notify, they are written in correct order
        int32_t revVer = bswap_32(bTemplate.version);
        int64_t revMinTime = bswap_32(bTemplate.minTime);
        bitsUint = bswap_32(bitsUint);

        size_t len =
            snprintf(notify_buff, MAX_NOTIFY_MESSAGE_SIZE,
                     "{\"id\":null,\"method\":\"mining.notify\",\"params\":"
                     "[\"%.8s\",\"%08x\",\"%.64s\",\"%.64s\",\"%.64s\",\"%"
                     "08x\",\"%08x\",%s,\"%.144s\"]}\n",
                     GetId().data(), revVer, prevBlockRevHex, merkleRootHex,
                     finalSRootRevHex, (uint32_t)revMinTime, bitsUint,
                     BoolToCstring(clean), bTemplate.solution.data());
        notify_buff_sv = std::string_view(notify_buff, len);

        memcpy(coinbase_tx_id, bTemplate.txList.transactions[0].data_hex.data(),
               HASH_SIZE);
    }

    template <typename T>
    inline void Write(void* dest, T* ptr, size_t size) const
    {
        memcpy(dest, ptr, size);
    }
    template <typename T>
    inline void WriteStatic(size_t offset, T* ptr, size_t size)
    {
        Write(static_header_data + offset, ptr, size);
    }

    template <typename T>
    inline void WriteUnhex(uint8_t* dest, T* ptr, size_t size) const
    {
        Unhexlify(dest, ptr, size);

    }

    template <typename T>
    inline void WriteUnhexStatic(size_t offset, T* ptr, size_t size)
    {
        WriteUnhex(static_header_data + offset, ptr, size);
    }

    void GetHeaderData(uint8_t* buff, std::string_view time,
                       std::string_view nonce1, std::string_view nonce2,
                       std::string_view sol) const /* override */
    {
        memcpy(buff, this->static_header_data, BLOCK_HEADER_STATIC_SIZE);

        constexpr int time_pos =
            VERSION_SIZE + PREVHASH_SIZE + MERKLE_ROOT_SIZE + FINALSROOT_SIZE;

        WriteUnhex(buff + time_pos, time.data(), TIME_SIZE * 2);

        constexpr int nonce1_pos =
            time_pos + TIME_SIZE +
            BITS_SIZE;  // bits are already written from static

        WriteUnhex(buff + nonce1_pos, nonce1.data(), EXTRANONCE_SIZE * 2);

        constexpr int nonce2_pos = nonce1_pos + EXTRANONCE_SIZE;
        WriteUnhex(buff + nonce2_pos, nonce2.data(), EXTRANONCE2_SIZE * 2);

        constexpr int solution_pos = nonce2_pos + EXTRANONCE2_SIZE;
        WriteUnhex(buff + solution_pos, sol.data(),
                   (SOLUTION_LENGTH_SIZE + SOLUTION_SIZE) * 2);
    }
    uint8_t coinbase_tx_id[HASH_SIZE];

   private:
    // uint8_t static_header_data[BLOCK_HEADER_STATIC_SIZE];

    // char* GetVersion() { return version; }
    // char* GetPrevBlockhash() { return hashPrevBlock; }
    // char* GetTime() { return nTime; }
    // char* GetBits() { return nBits; }

   private:
    int written = 0;
};

using job_t = VerusJob;

#endif