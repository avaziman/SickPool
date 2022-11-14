#ifndef JOB_CRYPTONOTE_HPP_
#define JOB_CRYPTONOTE_HPP_
#include <byteswap.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <array>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "job.hpp"
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
    // /*const*/ std::string_view prev_hash;
    const std::string seed;
    const uint32_t height;
    const uint64_t target_diff;
    const double expected_hashes;
    const uint32_t block_size;
    const currency::block block;
    const uint64_t coinbase_value;
    const uint32_t tx_count;
    const std::array<uint8_t, 32> template_hash;

    explicit BlockTemplateCn() = default;
    BlockTemplateCn& operator=(BlockTemplateCn&&) = default;

    currency::block UnserializeBlock(std::string_view templateblob) const
    {
        currency::block res;

        std::string template_bin;
        template_bin.resize(templateblob.size() / 2);
        Unhexlify((uint8_t*)template_bin.data(), templateblob.data(),
                  templateblob.size());

        bool deres = t_unserializable_object_from_blob<currency::block>(
            res, template_bin);

        return res;
    }

    auto GetTemplateHash() const
    {
        std::string template_hash_blob = get_block_hashing_blob(block);

        return HashWrapper::CnFastHash(
            reinterpret_cast<uint8_t*>(template_hash_blob.data()),
            template_hash_blob.size());
    }

    explicit BlockTemplateCn(const BlockTemplateResCn& btemplate)
        :  // prev_hash(btemplate.prev_hash),
          seed(btemplate.seed),
          height(btemplate.height),
          target_diff(btemplate.difficulty),
          expected_hashes(
              GetExpectedHashes<ZanoStatic>(static_cast<double>(target_diff))),
          block_size(static_cast<uint32_t>(btemplate.blob.size() / 2)),
          block(UnserializeBlock(btemplate.blob)),
          coinbase_value(block.miner_tx.vout[0].amount),
          // include coinbase tx
          tx_count(static_cast<uint32_t>(block.tx_hashes.size() + 1)),
          template_hash(GetTemplateHash())
    {
    }
};

template <>
class Job<StratumProtocol::CN> : public BlockTemplateCn, public JobBase
{
   public:
    explicit Job<StratumProtocol::CN>(const BlockTemplateResCn& bTemplate,
                             bool clean = true)
        : BlockTemplateCn(bTemplate),
            // job id is the block template hash hex
          JobBase(HexlifyS(template_hash))
    {
    }

    void GetHeaderData(uint8_t* buff, const ShareCn& share,
                       std::string_view extranonce1) const /* override*/
    {
        // nothing to do
    }

    inline void GetBlockHex(std::string& res, uint64_t nonce) const
    {
        currency::block cp(this->block);
        cp.nonce = nonce;
        auto bin = t_serializable_object_to_blob(cp);
        Hexlify(res.data(), bin.data(), bin.size());
    }

    template <StaticConf confs>
    std::string GetWorkMessage(const StratumClient* cli, int id) const
    {
        // how many hashes to find a share, lower target = more hashes
        const double diff = confs.DIFF1 * (1 / cli->GetDifficulty());

        auto bin_target = DoubleToBin256(diff);
        auto hex_target = Hexlify(bin_target);
        std::string_view hex_target_sv(hex_target.data(), hex_target.size());

        return fmt::format(
            "{{\"jsonrpc\":\"2.0\",\"id\":{},\"result\":[\"0x{"
            "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}",
            id,
            std::string_view(this->id.data(),
                             this->id.size()),
            seed, hex_target_sv, height);
    }

    template <StaticConf confs>
    std::string GetWorkMessage(const StratumClient* cli) const
    {
        const double diff = confs.DIFF1 * cli->GetDifficulty();

        auto bin_target = DoubleToBin256(diff);
        auto hex_target = Hexlify(bin_target);
        std::string_view hex_target_sv(hex_target.data(), hex_target.size());

        return fmt::format(
            "{{\"jsonrpc\":\"2.0\",\"result\":[\"0x{"
            "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}",
            std::string_view(this->id.data(), this->id.size()), seed,
            hex_target_sv, height);
    }
    // std::array for const explicity
    // const std::array<char, sizeof(BlockTemplateCn::template_hash) * 2>
    //     block_template_hash_hex;
    // using block_template_hash_hex = 
};

using JobCryptoNote = Job<StratumProtocol::CN>;

#endif

// TODO: check if nonce2 hasnt change, reuse the coinbase txid?