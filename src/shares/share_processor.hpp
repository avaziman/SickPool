#ifndef SHARE_PROCESSOR_HPP_
#define SHARE_PROCESSOR_HPP_
#include <fmt/format.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <type_traits>

#include "cn/currency_core/basic_pow_helpers.h"
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
    static inline bool VerifyShareParams(ShareResult& result, const job_t* job,
                                         std::string_view given_time,
                                         const int64_t curtime)
    {
        uint32_t shareTime = HexToUint(given_time.data(), given_time.size());
// in btc the value in not reversed
#ifdef STRATUM_ZEC
        shareTime = bswap_32(shareTime);  // swap to little endian
#endif

        // int64_t minTime = job->min_time;
        // int64_t maxTime = curtime / 1000 + MAX_FUTURE_BLOCK_TIME;

        // if (shareTime < minTime || shareTime > maxTime)
        // {
        //     result.code = ResCode::UNKNOWN;
        //     result.message =
        //         fmt::format("Invalid nTime (min: {}, max: {}, given: {})",
        //                     minTime, maxTime, shareTime);
        //     result.difficulty =
        //         static_cast<double>(BadDiff::INVALID_SHARE_DIFF);
        //     return false;
        // }

        return true;
    }

    static constexpr std::string_view field_str = "ShareProcessor";
    static const Logger<field_str> logger;

    template <HashAlgo hash_algo>
    inline static void Process(ShareResult& result, StratumClient* cli,
                               WorkerContext* wc, const job_t* job,
                               const share_t& share, int64_t curTime)
    {
        // veirfy params before even hashing

        // if (!VerifyShareParams(result, job, share.time, curTime))
        // {
        //     return;
        // }

        uint8_t* headerData = wc->block_header;

        job->GetHeaderData(headerData, share, cli->extra_nonce_sv);

        // std::cout << "block header: " << std::endl;
        // PrintHex(headerData, BLOCK_HEADER_SIZE);

        if constexpr (hash_algo == HashAlgo::PROGPOWZ)
        {
            currency::get_block_longhash_sick(
                result.hash_bytes.data(), static_cast<uint64_t>(job->height),
                job->block_template_hash.data(), share.nonce);
            std::ranges::reverse(result.hash_bytes.begin(),
                                 result.hash_bytes.end());
        }
        else if constexpr (hash_algo == HashAlgo::VERUSHASH_V2b2)
        {
            // takes about 6-8 microseconds vs 8-12 on snomp
            HashWrapper::VerushashV2b2(result.hash_bytes.data(), headerData,
                                       BLOCK_HEADER_SIZE, &wc->hasher);
        }
        else if constexpr (hash_algo == HashAlgo::X25X)
        {
            HashWrapper::X25X(result.hash_bytes.data(), headerData,
                              BLOCK_HEADER_SIZE);
        }
        else
        {
            throw std::runtime_error("Missing hash function");
        }

        uint256 hash(result.hash_bytes);

        logger.Log<LogType::Debug>("Share hash: {} ", hash.GetHex());

        // take from the end as first will have zeros
        // convert to uint32, (this will lose data)

        if (auto shareEnd = *reinterpret_cast<uint32_t*>(hash.begin());
            !cli->SetLastShare(shareEnd, curTime))
        {
            result.code = ResCode::DUPLICATE_SHARE;
            result.message = "Duplicate share";
            result.difficulty =
                static_cast<double>(BadDiff::INVALID_SHARE_DIFF);
            return;
        }

        result.difficulty = DIFF1 / UintToArith256(hash).getdouble();

        if (result.difficulty >= job->target_diff) [[unlikely]]
        {
            result.code = ResCode::VALID_BLOCK;
            return;
        }
        else if (result.difficulty / cli->GetDifficulty() < 0.99)  // allow 5% below
        {
            result.code = ResCode::LOW_DIFFICULTY_SHARE;
            result.message =
                fmt::format("Low difficulty share of {}", result.difficulty);
            char blockHeaderHex[BLOCK_HEADER_SIZE * 2];
            Hexlify(blockHeaderHex, headerData, BLOCK_HEADER_SIZE);

            // logger.Log<LogType::Debug>(
            //             "Low difficulty share diff: {}, hash: {}",
            //             result.Diff, hash.GetHex().c_str());
            // logger.Log<LogType::Debug>(
            //             "Block header: {}", BLOCK_HEADER_SIZE * 2,
            // blockHeaderHex);
            return;
        }

        result.code = ResCode::VALID_SHARE;
    }
};
#endif