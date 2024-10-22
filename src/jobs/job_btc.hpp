#ifndef JOB_BTC_HPP_
#define JOB_BTC_HPP_
#include <byteswap.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <experimental/array>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "block_template.hpp"
#include "daemon_responses_sin.hpp"
#include "job_base_btc.hpp"
#include "share.hpp"
#include "static_config.hpp"

// struct BlockTemplateBtc : public BlockTemplate
// {
//     std::span<uint8_t> coinb1;
//     std::span<uint8_t> coinb2;

//     BlockTemplateBtc() = default;

//     template <typename Template>
//     explicit BlockTemplateBtc(const Template& rpct)
//     // extra tx space for coinbase + payment
//         : BlockTemplate(rpct.version, rpct.prev_block_hash,
//                         rpct.transactions.size() + 2, rpct.coinbase_value,
//                         rpct.min_time,
//                         HexToUint(rpct.bits.data(), BITS_SIZE * 2),
//                         rpct.height)
//     {
//     }
// };

struct BlockTemplateBtc
{
    struct StaticHeaderBtc
    {
        int32_t version;
        std::array<uint8_t, 32> prev_bhash;
    };

    explicit BlockTemplateBtc(const BlockTemplateResBtc& tRes)
        : coinbase_value(tRes.coinbase_value),
          min_time(tRes.min_time)
    {
        static_header.version = tRes.version;
        Unhexlify(static_header.prev_bhash.data(), tRes.prev_block_hash.data(),
                  tRes.prev_block_hash.size());

        std::ranges::reverse(prev_bhash);
    }

    StaticHeaderBtc static_header;
    const int64_t coinbase_value;
    const int64_t min_time;
};

template<>
class JobT<StratumProtocol::BTC> : public JobBaseBtc
{
   public:
    explicit JobBtc(std::string&& jobId,
                    const BlockTemplateResBtc& bTemplate, bool is_payment)
        : JobBaseBtc(std::move(jobId)),
          bits(bTemplate.bits),
          //   coinb1(bTemplate.coinb1.begin(), bTemplate.coinb1.end()),
          //   coinb2(bTemplate.coinb2.begin(), bTemplate.coinb2.end()),
          coinbase_size(bTemplate.coinb1.size() + bTemplate.coinb2.size() +
                        EXTRANONCE_SIZE + EXTRANONCE2_SIZE)
    {
        coinb1.assign(bTemplate.coinb1.begin(), bTemplate.coinb1.end());
        coinb2.assign(bTemplate.coinb2.begin(), bTemplate.coinb2.end());
        // reverse all numbers for block encoding
        char prev_block_rev_hex[PREVHASH_SIZE * 2];

        ReverseHex(prev_block_rev_hex, bTemplate.prev_block_hash.data(),
                   PREVHASH_SIZE * 2);

        // write header

        constexpr auto VERSION_OFFSET = 0;
        WriteStatic(VERSION_OFFSET, &bTemplate.version, VERSION_SIZE);

        constexpr auto PREVHASH_OFFSET = VERSION_OFFSET + VERSION_SIZE;
        WriteUnhexStatic(PREVHASH_OFFSET, prev_block_rev_hex,
                         PREVHASH_SIZE * 2);

        // values are not sent as in block encoding byte order (no need to
        // reverse) only generating notify message once for efficiency reverse
        // all numbers for notify, they are written in correct order uint32_t
        // ver_rev = bswap_32(bTemplate.version); uint32_t min_time_rev =
        // bswap_32(bTemplate.min_time); uint32_t bits_rev =
        // bswap_32(bTemplate.bits);

        // exclude coinbase tx, as every share has a different one
        std::vector<uint8_t> merkle_branches;
        // one extra for duplicate hash
        merkle_branches.reserve((tx_count + 1) * HASH_SIZE);

        for (int i = 0; i < tx_count; i++)
        {
            // (hashes are already in block encoding, (hash_hex is not))
            memcpy(merkle_branches.data() + i * HASH_SIZE,
                   bTemplate.tx_list.transactions[i].hash, HASH_SIZE);
        }

        merkle_steps.reserve(tx_count * HASH_SIZE);
        merkle_steps_count =
            MerkleTree::CalcSteps(merkle_steps, merkle_branches, tx_count);

        std::string coinb1_hex;
        coinb1_hex.resize(coinb1.size() * 2);
        std::string coinb2_hex;
        coinb2_hex.resize(coinb2.size() * 2);
        char prev_bhash_rev[HASH_SIZE_HEX];

        Hexlify(coinb1_hex.data(), coinb1.data(), coinb1.size());
        Hexlify(coinb2_hex.data(), coinb2.data(), coinb2.size());

        // reverse* the prev hash
        for (int i = 0; i < HASH_SIZE_HEX / 8; i++)
        {
            memcpy(prev_bhash_rev + (i * 8),
                   bTemplate.prev_block_hash.data() +
                       ((HASH_SIZE_HEX / 8 - (i + 1)) * 8),
                   8);
        }

        // memcpy(coinbase_tx_id,
        //        bTemplate.tx_list.transactions[0].data_hex.data(), HASH_SIZE);
    }

    std::string GenNotifyMsg() const override
    {
        std::vector<std::string> merkle_steps_str(merkle_steps_count);

        for (int i = 0; i < merkle_steps_count; i++)
        {
            merkle_steps_str[i].resize(HASH_SIZE_HEX);
            Hexlify(merkle_steps_str[i].data(),
                    merkle_steps.data() + i * HASH_SIZE, HASH_SIZE);
        }

        return fmt::format(
                   "{{\"id\":null,\"method\":\"mining.notify\",\"params\":"
                   "[\"{}\",\"{}\",\"{}\",\"{}\",{},\"{:08x}\",\"{:08x}\","
                   "\"{:08x}\","
                   "{}]}}\n",
                   GetId(),  // job id
                   std::string_view(prev_bhash_rev,
                                    HASH_SIZE_HEX),  // prev hash
                   coinb1_hex,                       // coinb1
                   coinb2_hex,                       // coinb2
                   merkle_steps_str,                 // merkle branches
                   bTemplate.version, bTemplate.bits, this->min_time, clean)
            .size;
    }

    template <typename T>
    inline void Write(void* dest, T* ptr, size_t size) const
    {
        memcpy(dest, ptr, size);
    }
    template <typename T>
    inline void WriteUnhex(uint8_t* dest, T* ptr, size_t size) const
    {
        Unhexlify(dest, ptr, size);
    }

    void GetHeaderData(uint8_t* buff, const ShareBtc& share,
                       std::string_view extranonce1) const override
    {
        uint8_t cbtxid[HASH_SIZE];
        auto coinbase_bin = std::make_unique<uint8_t[]>(coinbase_size);

        auto cb_offset = 0;
        // coinb1
        memcpy(coinbase_bin.get(), coinb1.data(), coinb1.size());
        cb_offset += coinb1.size();

        // extranonce1
        Unhexlify(coinbase_bin.get() + cb_offset, extranonce1.data(),
                  EXTRANONCE_SIZE * 2);
        cb_offset += EXTRANONCE_SIZE;

        // extranonce2
        Unhexlify(coinbase_bin.get() + cb_offset, share.extranonce2.data(),
                  EXTRANONCE2_SIZE * 2);
        cb_offset += EXTRANONCE2_SIZE;

        // coinb2
        memcpy(coinbase_bin.get() + cb_offset, coinb2.data(), coinb2.size());

        // insert the coinbase tx id
        HashWrapper::SHA256d(cbtxid, coinbase_bin.get(), coinbase_size);

        // generate the block header:
        memcpy(buff, static_header_data, BLOCK_HEADER_STATIC_SIZE);

        // generate the merkle root
        constexpr auto MERKLE_ROOT_POS = VERSION_SIZE + PREVHASH_SIZE;
        MerkleTree::CalcRootFromSteps(buff + MERKLE_ROOT_POS, cbtxid,
                                      merkle_steps, merkle_steps_count);

        constexpr auto TIME_POS = MERKLE_ROOT_POS + MERKLE_ROOT_SIZE;
        Unhexlify(buff + TIME_POS, share.time.data(), TIME_SIZE * 2);
        *(uint32_t*)(buff + TIME_POS) =
            bswap_32(*((uint32_t*)(buff + TIME_POS)));

        constexpr auto BITS_POS = TIME_POS + TIME_SIZE;
        memcpy(buff + BITS_POS, &bits, BITS_SIZE);

        constexpr auto NONCE_POS = BITS_POS + BITS_SIZE;
        Unhexlify(buff + NONCE_POS, share.nonce.data(), NONCE_SIZE * 2);
        *(uint32_t*)(buff + NONCE_POS) =
            bswap_32(*((uint32_t*)(buff + NONCE_POS)));
    }

    inline void GetBlockHex(char* res, const uint8_t* header,
                            const std::string_view extra_nonce1,
                            const std::string_view extra_nonce2) const
    {
        Hexlify(res, header, BLOCK_HEADER_SIZE);
        memcpy(res + (BLOCK_HEADER_SIZE * 2), txs_hex.data(), txs_hex.size());
        // emplace the coinbase data hex
        memcpy(res + (BLOCK_HEADER_SIZE + coinb1.size() + tx_vi_length) * 2,
               extra_nonce1.data(), EXTRANONCE_SIZE * 2);
        memcpy(res + (BLOCK_HEADER_SIZE + coinb1.size() + tx_vi_length +
                      EXTRANONCE_SIZE) *
                         2,
               extra_nonce2.data(), EXTRANONCE2_SIZE * 2);
    }
    uint8_t coinbase_tx_id[HASH_SIZE];

   private:
    const uint32_t bits;

    // exluding coinbase tx
    /*const*/ std::vector<uint8_t> coinb1;
    /*const*/ std::vector<uint8_t> coinb2;
    const std::size_t coinbase_size;

    // char vector so we don't need to copy to calculate merkle root
    const std::vector<uint8_t> merkle_steps;
    int merkle_steps_count;
};

using job_t = JobBtc;

#endif

// TODO: check if nonce2 hasnt change, reuse the coinbase txid?