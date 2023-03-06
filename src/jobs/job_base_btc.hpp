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

// has static notify message
class JobBaseBtc : public JobBase
{
   public:
    explicit JobBaseBtc(std::string&& jobId, std::string&& notify,
                        const std::vector<TxRes>& res, bool clean = true)
        : JobBase(std::move(jobId), clean),
          notify_msg(std::move(notify)),
          transactions_hex(GetTransactionHex(res))
    {
    }
    
    const std::string notify_msg;
    const std::string transactions_hex;

   private:
    std::string GetTransactionHex(const std::vector<TxRes>& txs) const
    {
        std::string res;

        auto num_bytes = GenNumScript(txs.size());
        res += HexlifyS(num_bytes);
        for (const auto& tx : txs) res += tx.data;

        return res;
    }
};

#endif