#ifndef JOB_CRYPTONOTE_HPP_
#define JOB_CRYPTONOTE_HPP_
#include <byteswap.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <experimental/array>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "block_template.hpp"
#include "cn/currency_core/currency_basic.h"
#include "currency_format_utils_blocks.h"
#include "daemon_responses_cryptonote.hpp"
#include "job_base_btc.hpp"
#include "serialization/serialization.h"
#include "share.hpp"
#include "static_config.hpp"
#include "utils.hpp"
#include "stratum_client.hpp"

#pragma pack(push, 1)
struct BlockHeaderCN
{
    uint8_t major_version;
    uint8_t minor_version;
    uint64_t timestamp;
    std::string_view prev_id;
    uint64_t nonce;
    uint8_t flags;
};
#pragma pack(pop)

struct BlockTemplateCn
{
    uint8_t template_hash[HASH_SIZE];
    /*const*/ std::string_view prev_hash;
    /*const*/ std::string_view seed;
    /*const*/ uint32_t height;
    /*const*/ uint64_t target_diff;
    /*const*/ double expected_hashes;
    /*const*/ uint32_t block_size;
    uint64_t coinbase_value;
    uint32_t tx_count;

    // BlockTemplateCn& operator=(const BlockTemplateCn&& other){
    //     prev_hash = other.prev_hash;
    //     seed = other.seed;
    //     height = other.height;
    //     target_diff = other.target_diff;
    //     expected_hashes = other.expected_hashes;
    //     block_size = other.block_size;
    //     coinbase_value = other.coinbase_value;
    //     tx_count = other.tx_count;
    // }
    // BlockTemplateCn& operator=(const BlockTemplateCn& other) = default;
    BlockTemplateCn()
        : prev_hash(""),
          seed(""),
          height(0),
          target_diff(0),
          expected_hashes(0.d),
          block_size(0)
    {
    }

    BlockTemplateCn(const BlockTemplateResCn& btemplate)
        : seed(btemplate.seed),
          prev_hash(btemplate.prev_hash),
          height(btemplate.height),
          target_diff(btemplate.difficulty),
          expected_hashes(GetExpectedHashes(target_diff)),
          block_size(btemplate.blob.size() / 2)
    {
        std::string template_bin;
        template_bin.resize(btemplate.blob.size() / 2);
        Unhexlify((unsigned char*)template_bin.data(), btemplate.blob.data(),
                  btemplate.blob.size());

        currency::block cnblock;
        bool res = t_unserializable_object_from_blob<currency::block>(
            cnblock, template_bin);

        coinbase_value = cnblock.miner_tx.vout[0].amount;
        tx_count = cnblock.tx_hashes.size() + 1;

        std::string template_hash_blob = get_block_hashing_blob(cnblock);

        HashWrapper::CnFastHash(template_hash,
                                (uint8_t*)template_hash_blob.data(),
                                template_hash_blob.size());
    }
};

class JobCryptoNote : public JobBase
{
   public:
    JobCryptoNote(const std::string& jobId, const BlockTemplateCn& bTemplate,
                  bool clean = true)
        : JobBase(jobId, bTemplate),
          height(bTemplate.height),
          target_diff(bTemplate.target_diff),
          block_template_hash(bTemplate.template_hash,
                              bTemplate.template_hash + HASH_SIZE),
          seed_hash(bTemplate.seed)
    {
        Hexlify(block_template_hash_hex, bTemplate.template_hash, HASH_SIZE);

        // ReverseHex(prev_block_rev_hex, bTemplate.prev_block_hash.data(),
        //            PREVHASH_SIZE * 2);

        // write header

        // constexpr auto VERSION_OFFSET = 0;
        // WriteStatic(VERSION_OFFSET, &bTemplate.version, VERSION_SIZE);

        // constexpr auto PREVHASH_OFFSET = VERSION_OFFSET + VERSION_SIZE;
        // WriteUnhexStatic(PREVHASH_OFFSET, prev_block_rev_hex,
        //                  PREVHASH_SIZE * 2);

        // memcpy(coinbase_tx_id,
        //        bTemplate.tx_list.transactions[0].data_hex.data(),
        //        HASH_SIZE);
    }

    void GetHeaderData(uint8_t* buff, const ShareCn& share,
                       std::string_view extranonce1) const /* override*/
    {
    }

    inline void GetBlockHex(char* res, const uint8_t* header,
                            const std::string_view extra_nonce1,
                            const std::string_view extra_nonce2) const
    {
        // Hexlify(res, header, BLOCK_HEADER_SIZE);
        // memcpy(res + (BLOCK_HEADER_SIZE * 2), txs_hex.data(),
        // txs_hex.size());
        // // emplace the coinbase data hex
        // memcpy(res + (BLOCK_HEADER_SIZE + coinb1.size() + tx_vi_length) * 2,
        //        extra_nonce1.data(), EXTRANONCE_SIZE * 2);
        // memcpy(res + (BLOCK_HEADER_SIZE + coinb1.size() + tx_vi_length +
        //               EXTRANONCE_SIZE) *
        //                  2,
        //        extra_nonce2.data(), EXTRANONCE2_SIZE * 2);
    }
    uint8_t coinbase_tx_id[HASH_SIZE];

    std::size_t GetWorkMessage(char* buff, StratumClient* cli, int id) const
    {
        uint32_t diffBits = DiffToBits(cli->GetDifficulty());
        arith_uint256 arith256;
        arith256.SetCompact(diffBits);

        size_t len =
            fmt::format_to_n(
                buff, MAX_NOTIFY_MESSAGE_SIZE,
                "{{\"jsonrpc\":\"2.0\",\"id\":{},\"result\":[\"0x{"
                "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}",
                id, std::string_view(block_template_hash_hex, HASH_SIZE_HEX),
                seed_hash, arith256.GetHex(), height)
                .size;

        return len;
    }

    std::size_t GetWorkMessage(char* buff, StratumClient* cli) const
    {
        uint32_t diffBits = DiffToBits(cli->GetDifficulty());
        arith_uint256 arith256;
        arith256.SetCompact(diffBits);

        size_t len =
            fmt::format_to_n(
                buff, MAX_NOTIFY_MESSAGE_SIZE,
                "{{\"jsonrpc\":\"2.0\",\"result\":[\"0x{"
                "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}",
                std::string_view(block_template_hash_hex, HASH_SIZE_HEX),
                seed_hash, arith256.GetHex(), height)
                .size;

        return len;
    }
    const uint64_t target_diff;
    const uint32_t height;
    // const std::string block_template_hash;
    const std::string block_template_hash;
    char block_template_hash_hex[HASH_SIZE_HEX];

   private:
    const std::string seed_hash;
};

using job_t = JobCryptoNote;

#endif

// TODO: check if nonce2 hasnt change, reuse the coinbase txid?