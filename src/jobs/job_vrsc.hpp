#ifndef JOB_VRSC_HPP_
#define JOB_VRSC_HPP_
#include <byteswap.h>
#include <fmt/format.h>

#include <array>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <string>

#include "block_template.hpp"
#include "config_vrsc.hpp"
#include "constants.hpp"
#include "daemon_manager_vrsc.hpp"
#include "job.hpp"
#include "job_base_btc.hpp"
#include "merkle_tree.hpp"
#include "share.hpp"
#include "static_config.hpp"

struct BlockTemplateZec
{
    // ENCODED IN ORDER

    const uint32_t version; /* int32*/
    const std::array<uint8_t, HASH_SIZE> prev_block_hash;
    const std::array<uint8_t, HASH_SIZE> merkle_root_hash;
    const std::array<uint8_t, HASH_SIZE> final_sroot_hash;
    const uint32_t min_time;
    const uint32_t bits;

    const uint32_t height;
    const double target_diff;
    const uint64_t coinbase_value;
    const uint32_t tx_count;
    const double expected_hashes = 0;
    const uint32_t block_size = 0;

    // EVERYTHING AS IN BLOCK ENCODING
    explicit BlockTemplateZec(
        const DaemonManagerT<Coin::VRSC>::BlockTemplateRes& bTemplate)
        : version(bTemplate.version),
          prev_block_hash(
              UnhexlifyRev<HASH_SIZE * 2>(bTemplate.prev_block_hash)),
          merkle_root_hash(MerkleTree<32>::CalcRoot(
              MerkleTree<32>::GetHashes(bTemplate.transactions))),
          final_sroot_hash(
              UnhexlifyRev<HASH_SIZE * 2>(bTemplate.final_sroot_hash)),
          min_time(bTemplate.min_time),
          bits(bTemplate.bits),
          height(bTemplate.height),
          target_diff(HexToDouble(bTemplate.target)),
          coinbase_value(bTemplate.coinbase_value),
          tx_count(static_cast<uint32_t>(bTemplate.transactions.size()))
    {
    }

    // only generating notify message once for efficiency
    std::string GenerateNotifyMessage(std::string_view jobid,
                                      std::string_view solution,
                                      bool clean = true) const
    {
        // reverse all numbers for notify, they are written in correct order

        return fmt::format(
            "{{\"id\":null,\"method\":\"mining.notify\",\"params\":"
            "[\"{}\",\"{:08x}\",\"{}\",\"{}\",\"{}\",\"{"
            ":08x}\",\"{:08x}\",{},\"{}\"]}}\n",
            jobid,  // job id
            bswap_32(version), HexlifyS(prev_block_hash),
            HexlifyS(merkle_root_hash), HexlifyS(final_sroot_hash),
            bswap_32(min_time), bswap_32(bits), clean,
            std::string_view(solution.data(), 144));
    }
};

template <>
class Job<StratumProtocol::ZEC>
    : public BlockTemplateZec, public JobBaseBtc, public CoinConstantsZec
{
   public:
    explicit Job<StratumProtocol::ZEC>(
        std::string&& jobId,
        const DaemonManagerT<Coin::VRSC>::BlockTemplateRes& bTemplate,
        bool is_payment)
        : BlockTemplateZec(bTemplate),
          JobBaseBtc(std::move(jobId),
                     GenerateNotifyMessage(jobId, bTemplate.solution),
                     bTemplate.transactions)

    {
        // difficulty is calculated from opposite byte encoding than in block

        // reverse all numbers for block encoding
        // std::array<char, PREVHASH_SIZE* 2> prev_block_rev_hex =
        //     ReverseHexArr(bTemplate.prev_block_hash);
        // std::array<char, FINALSROOT_SIZE* 2> final_sroot_hash_hex =
        //     ReverseHexArr(bTemplate.finals_root_hash);
    }

    void GetHeaderData(uint8_t* buff, const ShareZec& share,
                       uint32_t nonce1) const /* override */
    {
        // STATIC HEADER DATA (VERSION TO FINALSROOT)
        memcpy(buff, &this->version, VERSION_SIZE + HASH_SIZE * 3);

        constexpr int TIME_POS =
            VERSION_SIZE + PREVHASH_SIZE + MERKLE_ROOT_SIZE + FINALSROOT_SIZE;

        memcpy(buff + TIME_POS, &share.time, TIME_SIZE);

        constexpr int BITS_POS = TIME_POS + TIME_SIZE;
        memcpy(buff + BITS_POS, &this->bits, BITS_SIZE);

        constexpr int NONCE1_POS = BITS_POS + BITS_SIZE;
        memcpy(buff + NONCE1_POS, &nonce1, sizeof(nonce1));

        constexpr int NONCE2_POS =
            NONCE1_POS + StratumConstants::EXTRANONCE_SIZE;
        Unhexlify(buff + NONCE2_POS, share.nonce2_sv.data(),
                  EXTRANONCE2_SIZE * 2);

        constexpr int SOLUTION_POS = NONCE2_POS + EXTRANONCE2_SIZE;
        Unhexlify(buff + SOLUTION_POS, share.solution.data(),
                  (SOLUTION_LENGTH_SIZE + SOLUTION_SIZE) * 2);
    }

    inline void GetBlockHex(std::string& res, const uint8_t* block_header) const
    {
        res.resize(BLOCK_HEADER_SIZE * 2 + transactions_hex.size());
        Hexlify(res.data(), block_header, BLOCK_HEADER_SIZE);
        std::ranges::copy(transactions_hex,
                          res.begin() + (BLOCK_HEADER_SIZE * 2));
    }

    bool operator==(const Job<StratumProtocol::ZEC>& other) const
    {
        return this->merkle_root_hash == other.merkle_root_hash;
    }
};

#endif