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
#include "stratum_client.hpp"
#include "utils.hpp"

struct BlockTemplateCn
{
    std::array<uint8_t, HASH_SIZE> template_hash;
    /*const*/ std::string_view prev_hash;
    /*const*/ std::string_view seed;
    /*const*/ uint32_t height;
    /*const*/ uint64_t target_diff;
    /*const*/ double expected_hashes;
    /*const*/ uint32_t block_size;
    uint64_t coinbase_value;
    uint32_t tx_count;
    currency::block block;

    explicit BlockTemplateCn()
        : prev_hash(""),
          seed(""),
          height(0),
          target_diff(0),
          expected_hashes(0.d),
          block_size(0)
    {
    }

    explicit BlockTemplateCn(const BlockTemplateResCn& btemplate)
        : 
          prev_hash(btemplate.prev_hash),
          seed(btemplate.seed),
          height(btemplate.height),
          target_diff(btemplate.difficulty),
          expected_hashes(GetExpectedHashes(static_cast<double>(target_diff))),
          block_size(static_cast<uint32_t>(btemplate.blob.size() / 2))
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
        // include coinbase tx
        tx_count = static_cast<uint32_t>(cnblock.tx_hashes.size() + 1);

        std::string template_hash_blob = get_block_hashing_blob(cnblock);

        HashWrapper::CnFastHash(
            template_hash.data(),
            reinterpret_cast<uint8_t*>(template_hash_blob.data()),
            template_hash_blob.size());
    }
};

class JobCryptoNote : public JobBase
{
   public:
    JobCryptoNote(const std::string& jobId, BlockTemplateCn&& bTemplate,
                  bool clean = true)
        : JobBase(jobId, bTemplate),
          block_template_hash(std::move(bTemplate.template_hash)),
          block_template_hash_hex(Hexlify(block_template_hash)),
          seed_hash(bTemplate.seed),
          block(bTemplate.block)
    {
    }

    void GetHeaderData(uint8_t* buff, const ShareCn& share,
                       std::string_view extranonce1) const /* override*/
    {
        // nothing to do
    }

    inline void GetBlockHex(std::string& res, uint64_t nonce) const
    {
        this->block.nonce = nonce;
        auto bin = t_serializable_object_to_blob(block);
        Hexlify(res.data(), bin.data(), bin.size());
    }

    uint8_t coinbase_tx_id[HASH_SIZE];

    std::size_t GetWorkMessage(char* buff, const StratumClient* cli,
                               int id) const
    {
        uint32_t diffBits = DiffToBits(cli->GetDifficulty());
        arith_uint256 arith256;
        arith256.SetCompact(diffBits);

        size_t len =
            fmt::format_to_n(buff, MAX_NOTIFY_MESSAGE_SIZE,
                             "{{\"jsonrpc\":\"2.0\",\"id\":{},\"result\":[\"0x{"
                             "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}",
                             id,
                             std::string_view(block_template_hash_hex.data(),
                                              block_template_hash_hex.size()),
                             seed_hash, arith256.GetHex(), height)
                .size;

        return len;
    }

    std::size_t GetWorkMessage(char* buff, const StratumClient* cli) const
    {
        uint32_t diffBits = DiffToBits(cli->GetDifficulty());
        arith_uint256 arith256;
        arith256.SetCompact(diffBits);

        size_t len =
            fmt::format_to_n(buff, MAX_NOTIFY_MESSAGE_SIZE,
                             "{{\"jsonrpc\":\"2.0\",\"result\":[\"0x{"
                             "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}",
                             std::string_view(block_template_hash_hex.data(),
                                              block_template_hash_hex.size()),
                             seed_hash, arith256.GetHex(), height)
                .size;

        return len;
    }
    // std::array for const explicity
    const std::array<uint8_t, HASH_SIZE> block_template_hash;
    const std::array<char, HASH_SIZE_HEX> block_template_hash_hex;

   private:
    const std::string seed_hash;
    mutable currency::block block;
};

using job_t = JobCryptoNote;

#endif

// TODO: check if nonce2 hasnt change, reuse the coinbase txid?