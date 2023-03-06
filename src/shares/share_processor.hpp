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
    static constexpr std::string_view field_str = "ShareProcessor";
    static const Logger logger;

    template <StaticConf confs>
    inline static void Process(
        ShareResult& result, StratumClient* cli,
        WorkerContext<confs.BLOCK_HEADER_SIZE>* wc,
        const Job<confs.STRATUM_PROTOCOL>* job,
        const StratumShareT<confs.STRATUM_PROTOCOL>& share, int64_t curTime)
    {
        // veirfy params before even hashing
        if constexpr (confs.STRATUM_PROTOCOL == StratumProtocol::ZEC)
        {
            int64_t curtime_s = curTime / 1000;
            int64_t max_time = curtime_s + confs.MAX_FUTURE_BLOCK_TIME;

            if (share.time < job->min_time || share.time > max_time)
            {
                result.code = ResCode::UNKNOWN;
                result.message =
                    fmt::format("Invalid nTime (min: {}, max: {}, given: {})",
                                job->min_time, max_time, share.time);
                return;
            }
        }

        // HASH NEEDS TO BE IN LE
        if constexpr (confs.STRATUM_PROTOCOL != StratumProtocol::CN)
        {
            job->GetHeaderData(wc->block_header.data(), share,
                               cli->extra_nonce);
        }

        if constexpr (confs.HASH_ALGO == HashAlgo::PROGPOWZ)
        {
            currency::get_block_longhash_sick(
                result.hash_bytes.data(), static_cast<uint64_t>(job->height),
                job->template_hash.data(), share.nonce);
        }
        else if constexpr (confs.HASH_ALGO == HashAlgo::VERUSHASH_V2b2)
        {
            // takes about 6-8 microseconds vs 8-12 on snomp
            HashWrapper::VerushashV2b2(
                result.hash_bytes.data(), wc->block_header.data(),
                CoinConstantsZec::BLOCK_HEADER_SIZE, &wc->hasher);
        }
        else
        {
            throw std::invalid_argument("Missing hash function");
        }
        // convert to uint32, (this will lose data)
        // LE, least significant are first (NOT ZEROS)
        uint32_t share_start = 0;
        std::memcpy(&share_start, result.hash_bytes.begin(),
                    sizeof(share_start));

        if (!cli->SetLastShare(share_start, curTime))
        {
            result.code = ResCode::DUPLICATE_SHARE;
            result.message = "Duplicate share";
            return;
        }

        double raw_diff = BytesToDoubleLE(result.hash_bytes);
        result.difficulty = confs.DIFF1 / raw_diff;

#ifdef DEBUG
        logger.Log<LogType::Debug>("Share hash: {}, diff: {}",
                                   HexlifyS(result.hash_bytes),
                                   result.difficulty);
#endif

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
        }

        result.code = ResCode::VALID_SHARE;
    }
};
#endif