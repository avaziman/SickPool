#ifndef JOB_VRSC_HPP_
#define JOB_VRSC_HPP_
#include <byteswap.h>
#include <fmt/format.h>

#include <array>
#include <iomanip>
#include <sstream>
#include <string>

#include "block_template.hpp"
#include "config_vrsc.hpp"
#include "constants.hpp"
#include "daemon_manager_vrsc.hpp"
#include "job_base_btc.hpp"
#include "merkle_tree.hpp"
#include "share.hpp"
#include "static_config.hpp"

struct BlockTemplateZec
{
    // ENCODED IN ORDER

    const int32_t version;
    const std::array<uint8_t, HASH_SIZE> prev_block_hash;
    const std::array<uint8_t, HASH_SIZE> merkle_root_hash;
    const std::array<uint8_t, HASH_SIZE> final_sroot_hash;
    const uint64_t min_time;
    const uint32_t bits;

    const uint32_t height;
    const double target_diff;
    const uint64_t coinbase_value;
    const uint32_t tx_count;
    const double expected_hashes = 0;
    const uint32_t block_size = 0;

    explicit BlockTemplateZec(
        const DaemonManagerT<Coin::VRSC>::BlockTemplateRes& bTemplate)
        : version(bTemplate.version),
          prev_block_hash(Unhexlify<HASH_SIZE * 2>(bTemplate.prev_block_hash)),
          merkle_root_hash(MerkleTree::CalcRoot(
              MerkleTree::GetHashes(bTemplate.transactions))),
          final_sroot_hash(
              Unhexlify<HASH_SIZE * 2>(bTemplate.final_sroot_hash)),
          min_time(bTemplate.min_time),
          bits(bTemplate.bits),
          height(bTemplate.height),
          target_diff(0.0),
          coinbase_value(bTemplate.coinbase_value),
          tx_count(bTemplate.transactions.size())
    {
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
                     GenerateNotifyMessage(bTemplate.solution))

    {
        // difficulty is calculated from opposite byte encoding than in block

        // reverse all numbers for block encoding
        // std::array<char, PREVHASH_SIZE* 2> prev_block_rev_hex =
        //     ReverseHexArr(bTemplate.prev_block_hash);
        // std::array<char, FINALSROOT_SIZE* 2> final_sroot_hash_hex =
        //     ReverseHexArr(bTemplate.finals_root_hash);
    }

    // only generating notify message once for efficiency
    std::string GenerateNotifyMessage(std::string_view solution) const
    {
        // reverse all numbers for notify, they are written in correct order

        return fmt::format(
            "{{\"id\":null,\"method\":\"mining.notify\",\"params\":"
            "[\"{}\",\"{:08x}\",\"{}\",\"{}\",\"{}\",\"{"
            ":08x}\",\"{:08x}\",{},\"{}\"]}}\n",
            id,  // job id
            bswap_32(version), HexlifyS(prev_block_hash),
            HexlifyS(merkle_root_hash), HexlifyS(final_sroot_hash),
            bswap_32(min_time), bswap_32(bits), clean, solution);
    }

    void GetHeaderData(uint8_t* buff, const ShareZec& share,
                       std::string_view nonce1) const /* override */
    {
        // memcpy(buff, this->static_header_data, BLOCK_HEADER_STATIC_SIZE);

        // constexpr int time_pos =
        //     VERSION_SIZE + PREVHASH_SIZE + MERKLE_ROOT_SIZE +
        //     FINALSROOT_SIZE;

        // // WriteUnhex(buff + time_pos, share.time.data(), TIME_SIZE * 2);

        // constexpr int nonce1_pos =
        //     time_pos + TIME_SIZE +
        //     BITS_SIZE;  // bits are already written from static

        // // WriteUnhex(buff + nonce1_pos, nonce1.data(), EXTRANONCE_SIZE * 2);

        // constexpr int nonce2_pos = nonce1_pos + EXTRANONCE_SIZE;
        // // WriteUnhex(buff + nonce2_pos, share.nonce2.data(),
        // //            EXTRANONCE2_SIZE * 2);

        // constexpr int solution_pos = nonce2_pos + EXTRANONCE2_SIZE;
        // WriteUnhex(buff + solution_pos, share.solution.data(),
        //            (SOLUTION_LENGTH_SIZE + SOLUTION_SIZE) * 2);
    }
};

#endif