#ifndef JOB_MANAGER_HPP
#define JOB_MANAGER_HPP
#include <simdjson.h>

#include "../crypto/hash_wrapper.hpp"
#include "../crypto/verus_transaction.hpp"
#include "../logger.hpp"
#include "./job.hpp"
#include "block_template.hpp"
#include "../daemon/daemon_rpc.hpp"

using namespace simdjson;

#define TXVERSION_GROUP 0x892f2085
#define TXVERSION 0x04

class JobManager
{
   public:
    JobManager() : jobCount(0) {}

    Job* GetNewJob();
    int64_t jobCount;

    BlockTemplate ParseBlockTemplateJson(std::vector<char>& json);

   private:
    // multiple jobs can use the same block template, (append transactions only)
    BlockTemplate blockTemplate;
    ondemand::parser jsonParser;
    std::string coinbaseExtra = "SickPool is in the building.";

    VerusTransaction GetCoinbaseTx(int64_t value, uint32_t height, std::time_t);

    TransactionData GetCoinbaseTxData(int64_t value, uint32_t height, std::time_t);
};

#endif