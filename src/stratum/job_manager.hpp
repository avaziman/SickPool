#ifndef JOB_MANAGER_HPP
#define JOB_MANAGER_HPP
#include <simdjson.h>

#include "../crypto/hash_wrapper.hpp"
#include "../crypto/verus_transaction.hpp"
#include "../daemon/daemon_rpc.hpp"
#include "../logger.hpp"
#include "./job.hpp"
#include "./verus_job.hpp"
#include "block_template.hpp"

using namespace simdjson;

#define TXVERSION_GROUP 0x892f2085
#define TXVERSION 0x04

class JobManager
{
   public:
    JobManager() : jobCount(0), blockTemplate() {}

    job_t* GetNewJob();

    BlockTemplate ParseBlockTemplateJson(std::vector<char>& json);

   private:
    // multiple jobs can use the same block template, (append transactions only)
    int64_t jobCount;
    BlockTemplate blockTemplate;
    ondemand::parser jsonParser;
    std::string coinbaseExtra = "SickPool is in the building.";

    VerusTransaction GetCoinbaseTx(int64_t value,
                                   uint32_t height, int64_t,
                                   std::string_view rpc_coinbase);

    TransactionData GetCoinbaseTxData(int64_t value, uint32_t height, int64_t,
                                      std::string_view rpc_coinbase);
};

#include "stratum_server.hpp" // TODO: VERY UGLY FIX

#endif