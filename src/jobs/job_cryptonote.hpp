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
#include "currency_format_utils_abstract.h"
#include "currency_format_utils_blocks.h"
#include "daemon_manager_zano.hpp"
#include "job.hpp"
#include "job_base_btc.hpp"
#include "serialization/serialization.h"
#include "share.hpp"
#include "static_config.hpp"
#include "stratum_client.hpp"
#include "utils.hpp"
using BlockTemplateResCn = DaemonManagerT<Coin::ZANO>::BlockTemplateRes;

struct BlockTemplateCn
{
    using HashT = std::array<uint8_t, 32>;
    const std::string block_bin;
    const std::string seed;
    const uint32_t height;
    const double target_diff;
    const double expected_hashes;
    const uint32_t block_size;
    const uint64_t coinbase_value;
    const uint32_t tx_count;
    const HashT template_hash;
    const uint64_t tx_sum;

    currency::block UnserializeBlock(const std::string& template_bin) const
    {
        currency::block res;

        t_unserializable_object_from_blob<currency::block>(res, template_bin);

        return res;
    }

    HashT GetTemplateHash(const currency::block& block) const
    {
        std::string template_hash_blob =
            currency::get_block_hashing_blob(block);

        return HashWrapper::CnFastHash(
            reinterpret_cast<uint8_t*>(template_hash_blob.data()),
            template_hash_blob.size());
    }

    // used to determine if a block is new based on tx hashes, fast, not including coinbase tx incase it changes randomally
    uint64_t GetTxSum(const currency::block& block) const
    {
        uint64_t sum = 0;
        for (const auto& h : block.tx_hashes)
        {
            sum += reinterpret_cast<const uint32_t&>(h);
        }

        return sum;
    }

    explicit BlockTemplateCn(const BlockTemplateResCn& btemplate)
        : BlockTemplateCn(btemplate, UnhexlifyS(btemplate.blob))
    {
    }

    bool operator==(const BlockTemplateCn& other) const
    {
        return this->template_hash == other.template_hash;
    }

   private:
    // private constructor to reuse block without storing it in struct
    explicit BlockTemplateCn(const BlockTemplateResCn& btemplate,
                             const std::string& block_bin)
        : BlockTemplateCn(btemplate, block_bin, UnserializeBlock(block_bin))
    {
    }

    explicit BlockTemplateCn(const BlockTemplateResCn& btemplate,
                             std::string_view block_bin,
                             const currency::block& block)
        : block_bin(block_bin),
          seed(btemplate.seed),
          height(btemplate.height),
          target_diff(static_cast<double>(btemplate.difficulty)),
          expected_hashes(GetHashMultiplier<ZanoStatic>() * target_diff),
          block_size(static_cast<uint32_t>(btemplate.blob.size() / 2)),
          coinbase_value(block.miner_tx.vout[0].amount),
          // include coinbase tx
          tx_count(static_cast<uint32_t>(block.tx_hashes.size() + 1)),
          template_hash(GetTemplateHash(block)),
          tx_sum(GetTxSum(block))
    {
    }
};

template <>
class Job<StratumProtocol::CN> : public BlockTemplateCn, public JobBase
{
   public:
    explicit Job<StratumProtocol::CN>(const BlockTemplateResCn& bTemplate,
                                      bool clean)
        : BlockTemplateCn(bTemplate),
          // job id is the block template hash hex
          JobBase(HexlifyS(template_hash), clean)
    {
    }

    // right after major_version (u8)
    static constexpr auto nonce_offset = 1;
    inline void GetBlockHex(std::string& res, const uint64_t nonce) const
    {
        std::string cp;
        cp.reserve(this->block_bin.size() * 2);

        // copy block bin
        cp = this->block_bin;
        // set found nonce
        char* nonce_pos = cp.data() + nonce_offset;
        std::copy(&nonce, &nonce + sizeof(nonce), nonce_pos);

        Hexlify(res.data(), cp.data(), cp.size());
    }

    std::array<uint8_t, 32> GetBlockHash(uint64_t nonce) const
    {
        std::array<uint8_t, 32> arr;
        std::string cp(this->block_bin);
        std::copy(&nonce, &nonce + sizeof(nonce), cp.data() + nonce_offset);

        currency::block block = UnserializeBlock(cp);
        *(reinterpret_cast<crypto::hash*>(arr.data())) = get_block_hash(block);
        return arr;
    }

    template <StaticConf confs>
    std::string GetWorkMessage(double diff, int64_t id) const
    {
        auto diff_hex = GetDifficultyHex<confs>(diff);
        std::string_view hex_target_sv(diff_hex.data(), diff_hex.size());

        return fmt::format(
            "{{\"jsonrpc\":\"2.0\",\"id\":{},\"result\":[\"0x{"
            "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}\n",
            id, std::string_view(this->id.data(), this->id.size()), seed,
            hex_target_sv, height);
    }

    template <StaticConf confs>
    std::string GetWorkMessage(double diff) const
    {
        auto diff_hex = GetDifficultyHex<confs>(diff);
        std::string_view hex_target_sv(diff_hex.data(), diff_hex.size());

        return fmt::format(
            "{{\"jsonrpc\":\"2.0\",\"result\":[\"0x{"
            "}\",\"0x{}\",\"0x{}\",\"0x{:016x}\"]}}\n",
            std::string_view(this->id.data(), this->id.size()), seed,
            hex_target_sv, height);
    }

    bool operator==(const Job<StratumProtocol::CN>& other) const
    {
        return this->tx_sum == other.tx_sum;
    }
};

using JobCryptoNote = Job<StratumProtocol::CN>;

#endif