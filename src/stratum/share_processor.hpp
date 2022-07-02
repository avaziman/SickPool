#include <chrono>
#include <iostream>

#include "../crypto/utils.hpp"
#include "../crypto/verushash/verus_hash.h"
#include "../logger.hpp"
#include "share.hpp"
#include "stratum_client.hpp"
#include "verus_job.hpp"

class ShareProcessor
{
   public:
    inline static void Process(int64_t curTime, StratumClient& cli,
                               const job_t& job, const Share& share,
                               ShareResult& result)
    {
        uint8_t* headerData = cli.GetBlockheaderBuff();

        if (!cli.GetIsAuthorized())
        {
            result.Code = ShareCode::UNAUTHORIZED_WORKER;
            result.Message = "Unauthorized worker";
            return;
        }

        // veirfy params before even hashing
        // convert share time to uint32 (fast)
        uint32_t shareTime = HexToUint(share.time.data(), share.time.size());
        shareTime = bswap_32(shareTime);  // swap to little endian

        int64_t minTime = job.GetMinTime();
        int64_t maxTime = curTime / 1000 + MAX_FUTURE_BLOCK_TIME;

        if (shareTime < minTime || shareTime > maxTime)
        {
            result.Code = ShareCode::UNKNOWN;
            result.Message =
                "Invalid nTime "
                "(min: " +
                std::to_string(minTime) + ", max: " + std::to_string(maxTime) +
                ", given: " + std::to_string(shareTime) + ")";
            return;
        }
        // TODO: verify solution

        job.GetHeaderData(headerData, share.time, cli.GetExtraNonce(),
                          share.nonce2, share.solution);

#if POOL_COIN <= COIN_VRSC
        // takes about 6-8 microseconds vs 8-12 on snomp
        HashWrapper::VerushashV2b2(result.HashBytes.data(), headerData,
                                   BLOCK_HEADER_SIZE, cli.GetHasher());
#endif
        uint256 hash(result.HashBytes);
        // arith_uint256 hashArith = UintToArith256(hash);

        // take from the end as first will have zeros
        // convert to uint32, (this will lose data)
        auto shareEnd = static_cast<uint32_t>(hash.GetCheapHash());

        if (!cli.SetLastShare(shareEnd, curTime))
        {
            result.Code = ShareCode::DUPLICATE_SHARE;
            result.Message = "Duplicate share";
            result.Diff = static_cast<double>(BadDiff::INVALID_SHARE_DIFF);
            return;
        }

        result.Diff = BitsToDiff(UintToArith256(hash).GetCompact(false));

        // if (hashArith >= *job.GetTarget())
        if (result.Diff >= job.GetTargetDiff())
        {
            result.Code = ShareCode::VALID_BLOCK;
            return;
        }
        else if (result.Diff / cli.GetDifficulty() < 1)  // allow 5% below
        {
            result.Code = ShareCode::LOW_DIFFICULTY_SHARE;
            result.Message = "Low difficulty share";
            Logger::Log(LogType::Debug, LogField::ShareProcessor,
                        "Low difficulty share diff: %f, hash: %s", result.Diff,
                        hash.GetHex().c_str());
            char blockHeaderHex[BLOCK_HEADER_SIZE * 2];
            Hexlify(blockHeaderHex, headerData, BLOCK_HEADER_SIZE);
            Logger::Log(LogType::Debug, LogField::ShareProcessor,
                        "Block header: %.*s", BLOCK_HEADER_SIZE * 2,
                        blockHeaderHex);
            return;
        }

        result.Code = ShareCode::VALID_SHARE;
        return;
    }
};