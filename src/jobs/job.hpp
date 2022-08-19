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
#include "utils.hpp"

class Job
{
   public:
    Job(const std::string& jobId, const BlockTemplate& bTemplate,
        bool is_payment = false)
        : job_id(jobId),
          blockReward(bTemplate.coinbaseValue),
          min_time(bTemplate.minTime),
          height(bTemplate.height),
          is_payment(is_payment)
    {
        // target.SetHex(std::string(bTemplate.target));

        tx_count = bTemplate.txList.transactions.size();
        txAmountByteValue = tx_count;

        txAmountByteLength = VarInt(txAmountByteValue);
        txs_Hex = std::vector<char>(
            (txAmountByteLength + bTemplate.txList.byteCount) * 2);
        Hexlify(txs_Hex.data(), (unsigned char*)&txAmountByteValue,
                txAmountByteLength);

        std::size_t written = txAmountByteLength * 2;
        for (const auto& txData : bTemplate.txList.transactions)
        {
            memcpy(txs_Hex.data() + written, txData.data_hex.data(),
                   txData.data_hex.size());
            written += txData.data_hex.size();
        }

        // Logger::Log(LogType::Debug, LogField::JobManager, "tx hex: %.*s",
        //             txsHex.size(), txsHex.data());
    }


    inline void GetBlockHex(const uint8_t* header, char* res) const
    {
        Hexlify(res, header, BLOCK_HEADER_SIZE);
        memcpy(res + (BLOCK_HEADER_SIZE * 2), txs_Hex.data(), txs_Hex.size());
    }

    int64_t GetBlockReward() const { return blockReward; }
    uint8_t* GetPrevBlockHash() { return static_header_data + VERSION_SIZE; }
    uint32_t GetHeight() const { return height; }
    size_t GetTransactionCount() const { return tx_count; }
    std::size_t GetBlockSize() const
    {
        return (BLOCK_HEADER_SIZE * 2) + txs_Hex.size();
    }
    std::string_view GetId() const { return std::string_view(job_id); }
    std::string_view GetNotifyMessage() const { return notify_buff_sv; }
    int64_t GetMinTime() const { return min_time; }
    double GetTargetDiff() const { return target_diff; }
    double GetEstimatedShares() const { return expected_shares; }
    bool GetIsPayment() const { return is_payment; }
    // arith_uint256* GetTarget() { return &target; }

   protected:
    const std::string job_id;
    uint8_t static_header_data[BLOCK_HEADER_STATIC_SIZE];
    char notify_buff[MAX_NOTIFY_MESSAGE_SIZE];
    std::string_view notify_buff_sv;
    const int64_t blockReward;
    /*const*/ double target_diff = 0;
    /*const*/ double expected_shares = 0;

   private:
    // arith_uint256 target;
    std::vector<char> txs_Hex;
    uint64_t txAmountByteValue;
    std::size_t txAmountByteLength;
    std::size_t tx_count;

    const int64_t min_time;
    const uint32_t height;
    const bool is_payment;
};

#if COIN == VRSC
#include "job_vrsc.hpp"
#endif
#endif