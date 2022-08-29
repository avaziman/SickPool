#ifndef JOB_HPP_
#define JOB_HPP_

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "block_template.hpp"
#include "merkle_tree.hpp"
#include "static_config.hpp"
#include "share.hpp"
#include "utils.hpp"

class Job
{
   public:
    Job(const std::string& jobId, const BlockTemplate& bTemplate,
        bool is_payment = false)
        : job_id(jobId),
          blockReward(bTemplate.coinbase_value),
          min_time(bTemplate.min_time),
          height(bTemplate.height),
          is_payment(is_payment),
          target_diff(BitsToDiff(bTemplate.bits)),
          expected_shares(GetExpectedHashes(this->target_diff) * BLOCK_TIME),
          tx_count(bTemplate.tx_list.transactions.size())
    {
        // target.SetHex(std::string(bTemplate.target));

        txAmountByteValue = tx_count;

        tx_vi_length = VarInt(txAmountByteValue);
        txs_hex = std::vector<char>(
            (tx_vi_length + bTemplate.tx_list.byteCount) * 2);
        Hexlify(txs_hex.data(), (unsigned char*)&txAmountByteValue,
                tx_vi_length);

        std::size_t written = tx_vi_length * 2;
        for (const auto& txData : bTemplate.tx_list.transactions)
        {
            memcpy(txs_hex.data() + written, txData.data_hex.data(),
                   txData.data_hex.size());
            written += txData.data_hex.size();
        }

        // Logger::Log(LogType::Debug, LogField::JobManager, "tx hex: {}",
        //             std::string_view(txs_hex.data(), txs_hex.size()));
    }

    // inline virtual void GetBlockHex(const WorkerContext* wc, char* res) const
    // {
    //     Hexlify(res, wc->block_header, BLOCK_HEADER_SIZE);
    //     memcpy(res + (BLOCK_HEADER_SIZE * 2), txs_Hex.data(), txs_Hex.size());
    // }

    int64_t GetBlockReward() const { return blockReward; }
    uint8_t* GetPrevBlockHash() { return static_header_data + VERSION_SIZE; }
    uint32_t GetHeight() const { return height; }
    size_t GetTransactionCount() const { return tx_count; }
    std::size_t GetBlockSizeHex() const
    {
        return (BLOCK_HEADER_SIZE * 2) + txs_hex.size();
    }
    std::string_view GetId() const { return std::string_view(job_id); }
    std::string_view GetNotifyMessage() const { return notify_buff_sv; }
    int64_t GetMinTime() const { return min_time; }
    double GetTargetDiff() const { return target_diff; }
    double GetEstimatedShares() const { return expected_shares; }
    bool GetIsPayment() const { return is_payment; }
    // arith_uint256* GetTarget() { return &target; }
    virtual void GetHeaderData(uint8_t* buff, const share_t& share,
                       std::string_view nonce1) const = 0;

   protected:
    const std::string job_id;
    uint8_t static_header_data[BLOCK_HEADER_STATIC_SIZE];
    char notify_buff[MAX_NOTIFY_MESSAGE_SIZE];
    std::string_view notify_buff_sv;
    const int64_t blockReward;
    const double target_diff = 0;
    const double expected_shares = 0;
    std::size_t tx_vi_length;
    std::vector<char> txs_hex;
    const std::size_t tx_count;

   private:
    // arith_uint256 target;
    uint64_t txAmountByteValue;

    const int64_t min_time;
    const uint32_t height;
    const bool is_payment;
};

#if COIN == VRSC
#include "job_vrsc.hpp"
#elif COIN == SIN
#include "job_btc.hpp"
#endif
#endif