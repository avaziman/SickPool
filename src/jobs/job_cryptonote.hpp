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

#include "block_template.hpp"
#include "cn/currency_core/currency_basic.h"
#include "currency_format_utils_blocks.h"
#include "daemon_responses_cryptonote.hpp"
#include "job.hpp"
#include "job_base_btc.hpp"
#include "serialization/serialization.h"
#include "share.hpp"
#include "static_config.hpp"
#include "stratum_client.hpp"
#include "utils.hpp"

struct BlockTemplateCn
{
    // /*const*/ std::string_view prev_hash;
    const std::string block_bin;
    const std::string seed;
    const uint32_t height;
    const uint64_t target_diff;
    const double expected_hashes;
    const uint32_t block_size;
    const uint64_t coinbase_value;
    const uint32_t tx_count;
    const std::array<uint8_t, 32> template_hash;

    explicit BlockTemplateCn() = default;
    BlockTemplateCn& operator=(BlockTemplateCn&&) = default;

    currency::block UnserializeBlock(const std::string& template_bin) const
    {
        currency::block res;

        bool deres = t_unserializable_object_from_blob<currency::block>(
            res, template_bin);

        return res;
    }

    auto GetTemplateHash(const currency::block& block) const
    {
        std::string template_hash_blob = currency::get_block_hashing_blob(block);

        return HashWrapper::CnFastHash(
            reinterpret_cast<uint8_t*>(template_hash_blob.data()),
            template_hash_blob.size());
    }

    explicit BlockTemplateCn(const BlockTemplateResCn& btemplate)
        :  // prev_hash(btemplate.prev_hash),
          BlockTemplateCn(btemplate, UnhexlifyS(btemplate.blob))
    {
    }

    bool operator==(const BlockTemplateCn& other) const {
        return this->template_hash == other.template_hash;
    }


   private:
    // private constructor to reuse block without storing it in struct
    explicit BlockTemplateCn(const BlockTemplateResCn& btemplate,
                             const std::string& block_bin)
        : BlockTemplateCn(btemplate, block_bin,
                          UnserializeBlock(block_bin))
    {
    }

    explicit BlockTemplateCn(const BlockTemplateResCn& btemplate,
                             std::string_view block_bin, const currency::block& block)
        :  // prev_hash(btemplate.prev_hash),
          block_bin(block_bin),
          seed(btemplate.seed),
          height(btemplate.height),
          target_diff(btemplate.difficulty),
          expected_hashes(
              GetExpectedHashes<ZanoStatic>(static_cast<double>(target_diff))),
          block_size(static_cast<uint32_t>(btemplate.blob.size() / 2)),
          coinbase_value(block.miner_tx.vout[0].amount),
          // include coinbase tx
          tx_count(static_cast<uint32_t>(block.tx_hashes.size() + 1)),
          template_hash(GetTemplateHash(block))
    {
    }
};

template <>
class Job<StratumProtocol::CN> : public BlockTemplateCn, public JobBase
{
   public:
    explicit Job<StratumProtocol::CN>(const BlockTemplateResCn& bTemplate)
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

    inline void GetBlockHex(std::string& res, const uint64_t nonce) const
    {
        constexpr auto nonce_offset = 1;  // right after major_version (u8)

        std::string cp(this->block_bin);
        *reinterpret_cast<uint64_t*>(cp.data() + nonce_offset) = nonce;

        res.resize(cp.size() * 2);
        Hexlify(res.data(), cp.data(), cp.size());
    }

    template <StaticConf confs>
    std::string GetWorkMessage(const StratumClient* cli, int id) const
    {
        auto diff_hex = GetDifficultyHex<confs>(cli->GetDifficulty());
        std::string_view hex_target_sv(diff_hex.data(), diff_hex.size());

        return fmt::format(
            "{{\"jsonrpc\":\"2.0\",\"id\":{},\"result\":[\"0x{"
            "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}",
            id, std::string_view(this->id.data(), this->id.size()), seed,
            hex_target_sv, height);
    }

    template <StaticConf confs>
    std::string GetWorkMessage(const StratumClient* cli) const
    {
        auto diff_hex = GetDifficultyHex<confs>(cli->GetDifficulty());
        std::string_view hex_target_sv(diff_hex.data(), diff_hex.size());

        return fmt::format(
            "{{\"jsonrpc\":\"2.0\",\"result\":[\"0x{"
            "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}",
            std::string_view(this->id.data(), this->id.size()), seed,
            hex_target_sv, height);
    }

    bool IsSameJob(const Job<StratumProtocol::CN>& other) {
        return this->template_hash == other.template_hash;
    }
};

using JobCryptoNote = Job<StratumProtocol::CN>;

#endif

// TODO: check if nonce2 hasnt change, reuse the coinbase txid?