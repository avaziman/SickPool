#include <fmt/format.h>

#include <chrono>
#include <iostream>

#include "jobs/job_manager.hpp"
#include "jobs/verus_job.hpp"
#include "logger.hpp"
#include "share.hpp"
#include "stratum_client.hpp"
#include "utils.hpp"
#include "verushash/verus_hash.h"

struct WorkerContext{
    uint32_t current_height;
    uint8_t block_header[BLOCK_HEADER_SIZE];
    CVerusHashV2 hasher = CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);
    simdjson::ondemand::parser json_parser;
};

class ShareProcessor
{
   public:
    inline static void Process(StratumClient* cli,
                               WorkerContext* wc, const job_t* job,
                               const Share& share, ShareResult& result,
                               int64_t curTime)
    {
        if (job == nullptr)
        {
            result.code = ShareCode::JOB_NOT_FOUND;
            result.message = "Job not found";
            return;
        }

        if (!cli->GetIsAuthorized())
        {
            result.code = ShareCode::UNAUTHORIZED_WORKER;
            result.message = "Unauthorized worker";
            return;
        }

        uint8_t* headerData = wc->block_header;

        // veirfy params before even hashing
        // convert share time to uint32 (fast)
        uint32_t shareTime = HexToUint(share.time.data(), share.time.size());
        shareTime = bswap_32(shareTime);  // swap to little endian

        int64_t minTime = job->GetMinTime();
        int64_t maxTime = curTime / 1000 + MAX_FUTURE_BLOCK_TIME;

        if (shareTime < minTime || shareTime > maxTime)
        {
            result.code = ShareCode::UNKNOWN;
            result.message =
                "Invalid nTime "
                "(min: " +
                std::to_string(minTime) + ", max: " + std::to_string(maxTime) +
                ", given: " + std::to_string(shareTime) + ")";
            return;
        }

        job->GetHeaderData(headerData, share.time, cli->GetExtraNonce(),
                           share.nonce2, share.solution);

#if POW_ALGO == POW_ALGO_VERUSHASH
        // takes about 6-8 microseconds vs 8-12 on snomp
        HashWrapper::VerushashV2b2(result.hash_bytes.data(), headerData,
                                   BLOCK_HEADER_SIZE, &wc->hasher);
#endif
        uint256 hash(result.hash_bytes);
        // arith_uint256 hashArith = UintToArith256(hash);

        // take from the end as first will have zeros
        // convert to uint32, (this will lose data)
        auto shareEnd = static_cast<uint32_t>(hash.GetCheapHash());

        if (!cli->SetLastShare(shareEnd, curTime))
        {
            result.code = ShareCode::DUPLICATE_SHARE;
            result.message = "Duplicate share";
            result.difficulty = static_cast<double>(BadDiff::INVALID_SHARE_DIFF);
            return;
        }

        result.difficulty = BitsToDiff(UintToArith256(hash).GetCompact(false));

        // if (hashArith >= *job->GetTarget())
        if (result.difficulty >= job->GetTargetDiff())
        {
            result.code = ShareCode::VALID_BLOCK;
            return;
        }
        else if (result.difficulty / cli->GetDifficulty() < 1)  // allow 5% below
        {
            result.code = ShareCode::LOW_DIFFICULTY_SHARE;
            result.message =
                fmt::format("Low difficulty share of {}", result.difficulty);
            char blockHeaderHex[BLOCK_HEADER_SIZE * 2];
            Hexlify(blockHeaderHex, headerData, BLOCK_HEADER_SIZE);

            // Logger::Log(LogType::Debug, LogField::ShareProcessor,
            //             "Low difficulty share diff: %f, hash: %s",
            //             result.Diff, hash.GetHex().c_str());
            // Logger::Log(LogType::Debug, LogField::ShareProcessor,
            //             "Block header: %.*s", BLOCK_HEADER_SIZE * 2,
            // blockHeaderHex);
            return;
        }

        result.code = ShareCode::VALID_SHARE;
        return;
    }
};