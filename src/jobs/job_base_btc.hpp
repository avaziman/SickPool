#ifndef JOB_BASE_BTC_HPP_
#define JOB_BASE_BTC_HPP_

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <shared_mutex>

#include "block_template.hpp"
#include "merkle_tree.hpp"
#include "static_config.hpp"
#include "share.hpp"
#include "utils.hpp"
#include "job_base.hpp"

class JobBaseBtc : public JobBase
{
   public:
    JobBaseBtc(const std::string& jobId, const BlockTemplate& bTemplate,
        bool is_payment = false)
        : JobBase(jobId, bTemplate),
          block_reward(bTemplate.coinbase_value),
          min_time(bTemplate.min_time),
          is_payment(is_payment),
          tx_count(bTemplate.tx_list.transactions.size())
    {
        // target.SetHex(std::string(bTemplate.target));

        std::size_t txAmountByteValue = tx_count;

        tx_vi_length = VarInt(txAmountByteValue);
        txs_hex = std::vector<char>(
            (tx_vi_length + bTemplate.tx_list.byte_count) * 2);
        Hexlify(txs_hex.data(), (unsigned char*)&txAmountByteValue,
                tx_vi_length);

        std::size_t written = tx_vi_length * 2;
        for (const auto& txData : bTemplate.tx_list.transactions)
        {
            memcpy(txs_hex.data() + written, txData.data_hex.data(),
                   txData.data_hex.size());
             written += txData.data_hex.size();
        }

        Logger::Log(LogType::Debug, LogField::JobManager, "tx hex: {}",
                    std::string_view(txs_hex.data(), txs_hex.size()));
    }

    // inline virtual void GetBlockHex(const WorkerContext* wc, char* res) const
    // {
    //     Hexlify(res, wc->block_header, BLOCK_HEADER_SIZE);
    //     memcpy(res + (BLOCK_HEADER_SIZE * 2), txs_Hex.data(), txs_Hex.size());
    // }

    uint8_t* GetPrevBlockHash() { return static_header_data + VERSION_SIZE; }
 
    // arith_uint256* GetTarget() { return &target; }
    // virtual void GetHeaderData(uint8_t* buff, const share_t& share,
    //                    std::string_view nonce1) const = 0;
    std::string_view GetNotifyMessage() const { return notify_buff_sv; }

    const bool is_payment;
    const int64_t min_time;
    const int64_t block_reward;
    const std::size_t tx_count;

   protected:
    uint8_t static_header_data[BLOCK_HEADER_STATIC_SIZE];
    std::size_t tx_vi_length;
    std::vector<char> txs_hex;

    char notify_buff[MAX_NOTIFY_MESSAGE_SIZE];
    std::string_view notify_buff_sv;

   private:
    // arith_uint256 target;
};

#if SICK_COIN == VRSC
#include "job_vrsc.hpp"
#elif SICK_COIN == SIN
#include "job_btc.hpp"
#elif SICK_COIN == ZANO
#include "job_cryptonote.hpp"
#endif
#endif