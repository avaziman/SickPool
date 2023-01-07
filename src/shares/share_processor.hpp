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
    //     template <StratumProtocol sp>
    //     static inline bool VerifyShareParams(ShareResult& result, const JobT*
    //     job,
    //                                          std::string_view given_time,
    //                                          const int64_t curtime);

    //     template <>
    //     static inline bool VerifyShareParams<StratumProtocol::CN>(
    //         ShareResult& result, const JobT* job, std::string_view
    //         given_time, const int64_t curtime)
    //     {
    //         uint32_t shareTime = static_cast<uint32_t>(
    //             HexToUint(given_time.data(), given_time.size()));
    // // in btc the value in not reversed
    // #ifdef STRATUM_ZEC
    //         shareTime = bswap_32(shareTime);  // swap to little endian
    // #endif

    //         uint64_t minTime = job->min_time;
    //         uint64_t maxTime = curtime / 1000 + MAX_FUTURE_BLOCK_TIME;

    //         if (shareTime < minTime || shareTime > maxTime)
    //         {
    //             result.code = ResCode::UNKNOWN;
    //             result.message =
    //                 fmt::format("Invalid nTime (min: {}, max: {}, given:
    //                 {})",
    //                             minTime, maxTime, shareTime);
    //             result.difficulty =
    //                 static_cast<double>(BadDiff::INVALID_SHARE_DIFF);
    //             return false;
    //         }

    //         return true;
    //     }

    static constexpr std::string_view field_str = "ShareProcessor";
    static const Logger<field_str> logger;

    // template <StratumProtocol sp, HashAlgo ha>
    // inline static void Hash(ShareResult& result, const Job<sp>* job, const
    // ShareT<sp> share);

    // template <StratumProtocol sp, HashAlgo ha>
    // inline static void Hash(ShareResult& result, const Job<sp>* job, const
    // ShareT<sp> share);

    template <StaticConf confs>
    inline static void Process(
        ShareResult& result, StratumClient* cli,
        WorkerContext<confs.BLOCK_HEADER_SIZE>* wc,
        const Job<confs.STRATUM_PROTOCOL>* job,
        const StratumShare<confs.STRATUM_PROTOCOL>& share, int64_t curTime)
    {
        // veirfy params before even hashing

        // if (!VerifyShareParams<confs.STRATUM_PROTOCOL>(result, job,
        // share.time,
        //                                                curTime))
        // {
        //     return;
        // }

        job->GetHeaderData(wc->block_header, share, cli->extra_nonce_sv);

        // std::cout << "block header: " << std::endl;
        // PrintHex(wc->block_header, BLOCK_HEADER_SIZE);

        if constexpr (confs.HASH_ALGO == HashAlgo::PROGPOWZ)
        {
            currency::get_block_longhash_sick(
                result.hash_bytes.data(), static_cast<uint64_t>(job->height),
                job->template_hash.data(), share.nonce);
        }
        else if constexpr (confs.HASH_ALGO == HashAlgo::VERUSHASH_V2b2)
        {
            // takes about 6-8 microseconds vs 8-12 on snomp
            // HashWrapper::VerushashV2b2(result.hash_bytes.data(),
            // wc->block_header,
            //                            BLOCK_HEADER_SIZE, &wc->hasher);
        }
        else if constexpr (confs.HASH_ALGO == HashAlgo::X25X)
        {
            // HashWrapper::X25X(result.hash_bytes.data(), wc->block_header,
            //                   BLOCK_HEADER_SIZE);
        }
        else
        {
            throw std::invalid_argument("Missing hash function");
        }

        #ifdef DEBUG
        logger.Log<LogType::Debug>("Share hash: {} ",
                                   HexlifyS(result.hash_bytes));
        #endif

        // take from the end as first will have zeros
        // convert to uint32, (this will lose data)

        if (auto shareEnd =
                *reinterpret_cast<uint32_t*>(result.hash_bytes.end() - sizeof(uint32_t));
            !cli->SetLastShare(shareEnd, curTime))
        {
            result.code = ResCode::DUPLICATE_SHARE;
            result.message = "Duplicate share";
            result.difficulty =
                static_cast<double>(BadDiff::INVALID_SHARE_DIFF);
            return;
        }

        double raw_diff = BytesToDouble(result.hash_bytes);
        result.difficulty = confs.DIFF1 / raw_diff;

        if (result.difficulty >= job->target_diff) [[unlikely]]
        {
            result.code = ResCode::VALID_BLOCK;
            return;
        }
        else if (result.difficulty / cli->GetDifficulty() <
                 0.99)  // allow 1% below
        {
            result.code = ResCode::LOW_DIFFICULTY_SHARE;
            result.message =
                fmt::format("Low difficulty share of {} (Expected: {})",
                            result.difficulty, cli->GetDifficulty());

            return;
        }

        result.code = ResCode::VALID_SHARE;
    }
};
#endif