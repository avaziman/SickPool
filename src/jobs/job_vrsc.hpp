#ifndef JOB_VRSC_HPP_
#define JOB_VRSC_HPP_
#include <byteswap.h>
#include <fmt/format.h>

#include <experimental/array>
#include <iomanip>
#include <sstream>
#include <string>

#include "block_template.hpp"
#include "job_base_btc.hpp"
#include "static_config.hpp"
#include "share.hpp"

class JobVrsc : public JobBaseBtc
{
   public:
    JobVrsc(const std::string& jobId, const BlockTemplateVrsc& bTemplate,
            bool is_payment, bool clean = true)
        : JobBaseBtc(jobId, bTemplate, is_payment)
    {
        char merkle_root_hex[MERKLE_ROOT_SIZE * 2];

        // difficulty is calculated from opposite byte encoding than in block

        // reverse all numbers for block encoding
        char prev_block_rev_hex[PREVHASH_SIZE * 2];
        char final_sroot_hash_hex[FINALSROOT_SIZE * 2];

        ReverseHex(prev_block_rev_hex, bTemplate.prev_block_hash.data(),
                   PREVHASH_SIZE * 2);
        ReverseHex(final_sroot_hash_hex, bTemplate.finals_root_hash.data(),
                   FINALSROOT_SIZE * 2);

        // write header

        constexpr auto VERSION_OFFSET = 0;
        WriteStatic(VERSION_OFFSET, &bTemplate.version, VERSION_SIZE);
        // *((uint32_t*)static_header_data[VERSION_OFFSET]) = BLOCK_VERSION;

        constexpr auto PREVHASH_OFFSET = VERSION_OFFSET + VERSION_SIZE;
        WriteUnhexStatic(PREVHASH_OFFSET, prev_block_rev_hex,
                         PREVHASH_SIZE * 2);

        // hashes are given in LE, no need to reverse
        constexpr auto MROOT_OFFSET = PREVHASH_OFFSET + PREVHASH_SIZE;
        MerkleTree::CalcRoot(static_header_data + MROOT_OFFSET,
                             bTemplate.tx_list.transactions);

        constexpr auto FSROOT_OFFSET = MROOT_OFFSET + MERKLE_ROOT_SIZE;
        WriteUnhexStatic(FSROOT_OFFSET, final_sroot_hash_hex,
                         FINALSROOT_SIZE * 2);

        // not static
        constexpr auto TIME_OFFSET = FSROOT_OFFSET + FINALSROOT_SIZE;

        constexpr auto BITS_OFFSET = TIME_OFFSET + TIME_SIZE;
        WriteStatic(BITS_OFFSET, &bTemplate.bits, BITS_SIZE);

        // we need the hexlified merkle root for the notification message
        Hexlify(merkle_root_hex, static_header_data + MROOT_OFFSET,
                MERKLE_ROOT_SIZE);

        // only write the sol size and 72 first bytes of sol
        // the rest is changed by the miner
        // unsigned char solLen[] = {0xfd, 0x40, 0x05};
        // written += 32; // skip nonce

        // WriteWriteUnhex(lution, 144);

        // only generating notify message once for efficiency

        // reverse all numbers for notify, they are written in correct order
        uint32_t ver_rev = bswap_32(bTemplate.version);
        uint32_t min_time_rev = bswap_32(bTemplate.min_time);
        uint32_t bits_rev = bswap_32(bTemplate.bits);

        size_t len =
            fmt::format_to_n(
                notify_buff, MAX_NOTIFY_MESSAGE_SIZE,
                "{{\"id\":null,\"method\":\"mining.notify\",\"params\":"
                "[\"{}\",\"{:08x}\",\"{}\",\"{}\",\"{}\",\"{"
                ":08x}\",\"{:08x}\",{},\"{}\"]}}\n",
                GetId(),  // job id
                ver_rev,
                std::string_view(prev_block_rev_hex,
                                 sizeof(prev_block_rev_hex)),  // prev hash
                std::string_view(merkle_root_hex,
                                 sizeof(merkle_root_hex)),  // merkle root
                std::string_view(final_sroot_hash_hex,
                                 sizeof(final_sroot_hash_hex)),  // finals root
                min_time_rev, bits_rev, clean, bTemplate.solution)
                .size;
        notify_buff_sv = std::string_view(notify_buff, len);

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

    void GetHeaderData(uint8_t* buff, const ShareZec& share,
                       std::string_view nonce1) const override
    {
        memcpy(buff, this->static_header_data, BLOCK_HEADER_STATIC_SIZE);

        constexpr int time_pos =
            VERSION_SIZE + PREVHASH_SIZE + MERKLE_ROOT_SIZE + FINALSROOT_SIZE;

        WriteUnhex(buff + time_pos, share.time.data(), TIME_SIZE * 2);

        constexpr int nonce1_pos =
            time_pos + TIME_SIZE +
            BITS_SIZE;  // bits are already written from static

        WriteUnhex(buff + nonce1_pos, nonce1.data(), EXTRANONCE_SIZE * 2);

        constexpr int nonce2_pos = nonce1_pos + EXTRANONCE_SIZE;
        WriteUnhex(buff + nonce2_pos, share.nonce2.data(), EXTRANONCE2_SIZE * 2);

        constexpr int solution_pos = nonce2_pos + EXTRANONCE2_SIZE;
        WriteUnhex(buff + solution_pos, share.solution.data(),
                   (SOLUTION_LENGTH_SIZE + SOLUTION_SIZE) * 2);
    }
};

using job_t = JobVrsc;

#endif