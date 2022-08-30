#ifndef SHARE_PROCESSOR_HPP_
#define SHARE_PROCESSOR_HPP_
#include <fmt/format.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>

#include "jobs/job_manager.hpp"
#include "logger.hpp"
#include "share.hpp"
#include "static_config/static_config.hpp"
#include "stratum_client.hpp"
#include "stratum_server.hpp"
#include "utils.hpp"
#include "verushash/verus_hash.h"

class ShareProcessor
{
   public:
    static inline bool VerifyShareParams(ShareResult& result, const job_t* job, std::string_view given_time,
                                  const int64_t curtime)
    {

        uint32_t shareTime = HexToUint(given_time.data(), given_time.size());
// in btc the value in not reversed
#ifdef STRATUM_ZEC
        shareTime = bswap_32(shareTime);  // swap to little endian
#endif

        int64_t minTime = job->GetMinTime();
        int64_t maxTime = curtime / 1000 + MAX_FUTURE_BLOCK_TIME;

        if (shareTime < minTime || shareTime > maxTime)
        {
            result.code = ResCode::UNKNOWN;
            result.message =
                fmt::format("Invalid nTime (min: {}, max: {}, given: {})",
                            minTime, maxTime, shareTime);
            result.difficulty = static_cast<double>(BadDiff::INVALID_SHARE_DIFF);
            return false;
        }

        return true;
    }

    inline static void Process(ShareResult& result, StratumClient* cli,
                               WorkerContext* wc, const job_t* job,
                               const share_t& share, int64_t curTime)
    {
        // veirfy params before even hashing

        if (!VerifyShareParams(result, job, share.time, curTime))
        {
            return;
        }

        uint8_t* headerData = wc->block_header;

        job->GetHeaderData(headerData, share, cli->GetExtraNonce());

        // std::cout << "block header: " << std::endl;
        // PrintHex(headerData, BLOCK_HEADER_SIZE);

        if constexpr (HASH_ALGO == HashAlgo::VERUSHASH_V2b2)
        {
            // takes about 6-8 microseconds vs 8-12 on snomp
            HashWrapper::VerushashV2b2(result.hash_bytes.data(), headerData,
                                       BLOCK_HEADER_SIZE, &wc->hasher);
        }
        else if constexpr (HASH_ALGO == HashAlgo::X25X)
        {
            HashWrapper::X25X(result.hash_bytes.data(), headerData,
                              BLOCK_HEADER_SIZE);
        }
        else
        {
            throw std::runtime_error("Missing hash function");
        }

        uint256 hash(result.hash_bytes);
        // Logger::Log(LogType::Debug, LogField::ShareProcessor, "Share hash:
        // {}",
        //             hash.GetHex());

        // arith_uint256 hashArith = UintToArith256(hash);

        // take from the end as first will have zeros
        // convert to uint32, (this will lose data)
        auto shareEnd = static_cast<uint32_t>(hash.GetCheapHash());

        if (!cli->SetLastShare(shareEnd, curTime))
        {
            result.code = ResCode::DUPLICATE_SHARE;
            result.message = "Duplicate share";
            result.difficulty =
                static_cast<double>(BadDiff::INVALID_SHARE_DIFF);
            return;
        }

        result.difficulty = BitsToDiff(UintToArith256(hash).GetCompact(false));

        // if (hashArith >= *job->GetTarget())
        if (unlikely(result.difficulty >= job->GetTargetDiff()))
        {
            result.code = ResCode::VALID_BLOCK;
            return;
        }
        else if (unlikely(result.difficulty / cli->GetDifficulty() <
                          1.))  // allow 5% below
        {
            result.code = ResCode::LOW_DIFFICULTY_SHARE;
            result.message =
                fmt::format("Low difficulty share of {}", result.difficulty);
            char blockHeaderHex[BLOCK_HEADER_SIZE * 2];
            Hexlify(blockHeaderHex, headerData, BLOCK_HEADER_SIZE);

            // Logger::Log(LogType::Debug, LogField::ShareProcessor,
            //             "Low difficulty share diff: {}, hash: {}",
            //             result.Diff, hash.GetHex().c_str());
            // Logger::Log(LogType::Debug, LogField::ShareProcessor,
            //             "Block header: {}", BLOCK_HEADER_SIZE * 2,
            // blockHeaderHex);
            return;
        }

        result.code = ResCode::VALID_SHARE;
        return;
    }
};
#endif