#ifndef BLOCK_SUBMISSION_HPP
#define BLOCK_SUBMISSION_HPP

#include <cinttypes>
#include <cstring>
#include <string>

#include "jobs/verus_job.hpp"
#include "shares/share.hpp"
#include "stats/round.hpp"

enum class BlockType : uint8_t
{
    POW = 0b1,
    POS = 0b10,
    PAYMENT = 0b100,
    POW_PAYMENT = POW | PAYMENT
};

#pragma pack(push, 1)
struct BlockSubmission
{
   public:
    BlockSubmission(const std::string_view chainsv,
                    const std::string_view worker_full,
                    const BlockType blocktype, const uint32_t height,
                    const int64_t reward, const Round& chain_round,
                    const int64_t time, const uint32_t number,
                    const double diff, const double effort_percent,
                    const uint8_t* hash)
        : block_type(blocktype),
          block_reward(reward),
          duration_ms(time - chain_round.round_start_ms),
          time_ms(time),
          height(height),
          number(number),
          difficulty(diff),
          effort_percent(effort_percent)
    {
        memcpy(miner, worker_full.data(), ADDRESS_LEN);
        memcpy(worker, worker_full.data() + ADDRESS_LEN + 1,
               worker_full.size() - ADDRESS_LEN - 1);
        memcpy(chain, chainsv.data(), chainsv.size());

        // reverse
        Hexlify((char*)hash_hex, hash, HASH_SIZE);
        ReverseHex((char*)hash_hex, (char*)hash_hex, HASH_SIZE_HEX);
    }

    const int32_t confirmations = 0;  // up to 100, changed in database
    const BlockType block_type;
    const int64_t block_reward;
    const int64_t time_ms;      // ms percision
    const int64_t duration_ms;  // ms percision
    const uint32_t height;
    const uint32_t number;
    const double difficulty;
    const double effort_percent;
    unsigned char chain[8] = {0};
    unsigned char miner[ADDRESS_LEN] = {0};
    unsigned char worker[MAX_WORKER_NAME_LEN] = {0};  // separated
    unsigned char hash_hex[HASH_SIZE_HEX] = {0};
};

// block submission with additional information for internal use only (not
// passed to db)
struct ExtendedSubmission : public BlockSubmission
{
   public:
    ExtendedSubmission(const std::string_view chainsv,
                       const std::string_view worker_full,
                       const BlockType blocktype, const uint32_t height,
                       const int64_t reward, const Round& chain_round,
                       const int64_t time, const uint32_t number,
                       const double diff, const double effort_percent,
                       const uint8_t* hash, const uint8_t* cb_txid)
        : chain_sv(chainsv),
          miner_sv(worker_full.data(), ADDRESS_LEN),
          BlockSubmission(chainsv, worker_full, blocktype, height, reward,
                          chain_round, time, number, diff, effort_percent, hash)
    {
        memcpy(coinbase_txid, cb_txid, sizeof(coinbase_txid));
    }
    // way easier to use than unsigned char pointer :)
    std::string_view chain_sv;
    std::string_view miner_sv;

    uint8_t coinbase_txid[HASH_SIZE] = {0};
};
#pragma pack(pop)

// don't pack

/* block submission attributes are
    sortable:
    time/number
    reward
    difficulty
    effort
    duration
    (no need height because it will always be grouped by chains, so you can just
   filter by chain and sort by time/number).

   non-sortable (filterable):

    chain
    type (pow/pos)
    solver address
*/

#endif