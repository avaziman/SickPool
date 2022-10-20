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

#include "cn/currency_core/currency_basic.h"
#include "block_template.hpp"
#include "currency_format_utils_blocks.h"
#include "daemon_responses_cryptonote.hpp"
#include "job_base_btc.hpp"
#include "serialization/serialization.h"
#include "share.hpp"
#include "static_config.hpp"
#include "stratum_client.hpp"
#include "utils.hpp"

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
    currency::block block;

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

        block = cnblock;

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
          seed_hash(bTemplate.seed),
          block(bTemplate.block)
    {
        memcpy(block_template_hash, bTemplate.template_hash, HASH_SIZE);
        Hexlify(block_template_hash_hex, bTemplate.template_hash, HASH_SIZE);

        // logger.Log<LogType::Info>( 
        //             "Blocktemplate hash hex: {}",
        //             std::string_view(block_template_hash_hex, HASH_SIZE_HEX));

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
        // nothing to do
    }


    inline void GetBlockHex(std::string& res, uint64_t nonce)
    {
        this->block.nonce = nonce;
        auto bin = t_serializable_object_to_blob(block);
        Hexlify(res.data(), bin.data(), bin.size());
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

        size_t len = fmt::format_to_n(buff, MAX_NOTIFY_MESSAGE_SIZE,
                                      "{{\"jsonrpc\":\"2.0\",\"result\":[\"0x{"
                                      "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}",
                                      std::string_view(block_template_hash_hex,
                                                       HASH_SIZE_HEX),
                                      seed_hash, arith256.GetHex(), height)
                         .size;

        return len;
    }
    const uint64_t target_diff;
    const uint32_t height;
    // const std::string block_template_hash;
    uint8_t block_template_hash[HASH_SIZE];
    char block_template_hash_hex[HASH_SIZE_HEX];
    // changes if we find block
    /*mutable*/ currency::block block;

   private:
    const std::string seed_hash;
};

using job_t = JobCryptoNote;

#endif

// TODO: check if nonce2 hasnt change, reuse the coinbase txid?