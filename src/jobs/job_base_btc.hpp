#ifndef JOB_BASE_BTC_HPP_
#define JOB_BASE_BTC_HPP_

#include <ctime>
#include <iomanip>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <vector>

#include "block_template.hpp"
#include "daemon_responses_btc.hpp"
#include "job_base.hpp"
#include "logger.hpp"
#include "merkle_tree.hpp"
#include "share.hpp"
#include "static_config.hpp"
#include "utils.hpp"

class JobBaseBtc : public JobBase
{
   public:
    explicit JobBaseBtc(std::string&& jobId,
                        std::string&& notify, bool is_payment = false)
        : JobBase(std::move(jobId)), notify_msg(std::move(notify))
    {
        // target.SetHex(std::string(bTemplate.target));

        // std::size_t txAmountByteValue = tx_count;

        // tx_vi_length = VarInt(txAmountByteValue);
        // txs_hex = std::vector<char>(
        //     (tx_vi_length + bTemplate.tx_list.byte_count) * 2);
        // Hexlify(txs_hex.data(), (unsigned char*)&txAmountByteValue,
        //         tx_vi_length);

        // std::size_t written = tx_vi_length * 2;
        // for (const auto& txData : bTemplate.tx_list.transactions)
        // {
        //     memcpy(txs_hex.data() + written, txData.data_hex.data(),
        //            txData.data_hex.size());
        //     written += txData.data_hex.size();
        // }

        // logger.Log<LogType::Debug>(  "tx hex: {}",
        //             std::string_view(txs_hex.data(), txs_hex.size()));
    }

    // inline virtual void GetBlockHex(const WorkerContext* wc, char* res) const
    // {
    //     Hexlify(res, wc->block_header, BLOCK_HEADER_SIZE);
    //     memcpy(res + (BLOCK_HEADER_SIZE * 2), txs_Hex.data(),
    //     txs_Hex.size());
    // }

    // uint8_t* GetPrevBlockHash()
    // {
    //     return static_header_data + sizeof(this->version);
    // }

    // arith_uint256* GetTarget() { return &target; }
    // virtual void GetHeaderData(uint8_t* buff, const share_t& share,
    //                    std::string_view nonce1) const = 0;

   private:
    const std::string notify_msg;
};

#endif