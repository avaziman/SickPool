#include <chrono>
#include <iostream>

#include "../crypto/utils.hpp"
#include "../crypto/verushash/verus_hash.h"
#include "../logger.hpp"
#include "job.hpp"
#include "share.hpp"
#include "stratum_client.hpp"

using namespace std::chrono;

class ShareProcessor
{
   public:
    static ShareResult Process(std::time_t curTime, StratumClient& cli,
                               Job& job, const Share& share)
    {
        ShareResult result;
        unsigned char* headerData;

        // veirfy params before even hashing
        // convert share time to uint32 (fast)
        uint32_t shareTime = HexToUint(share.time.data(), share.time.size());
        shareTime = bswap_32(shareTime);  // swap to little endian

        uint32_t minTime = job.GetMinTime();
        uint32_t maxTime = curTime / 1000 + MAX_FUTURE_BLOCK_TIME;

        if (shareTime < minTime || shareTime > maxTime)
        {
            result.Code = ShareCode::UNKNOWN;
            result.Message = "Invalid nTime";
            return result;
        }
        // TODO: verify solution

        headerData = job.GetHeaderData(share.time, cli.GetExtraNonce(),
                                       share.nonce2, share.solution);
        
#if POOL_COIN <= COIN_VRSC
        // takes about 6-8 microseconds vs 8-12 on snomp
        HashWrapper::VerushashV2b2(result.HashBytes.data(), headerData,
                                   BLOCK_HEADER_SIZE, cli.GetHasher());
#endif
        uint256 hash(result.HashBytes);
        arith_uint256 hashArith = UintToArith256(hash);
        // Logger::Log(LogType::Debug, LogField::ShareProcessor, "Share hash: %s",
        //             hash.GetHex().c_str());

        // take from the end as first will have zeros
        // convert to uint32, (this will lose data)
        uint32_t shareEnd = hash.GetCheapHash();
        // std::cout << std::hex << shareEnd << std::endl;
        if (!cli.SetLastShare(shareEnd, curTime))
        {
            result.Code = ShareCode::DUPLICATE_SHARE;
            result.Message = "Duplicate share";
            return result;
        }

        result.Diff = BitsToDiff(UintToArith256(hash).GetCompact(false));

        // if (hashArith >= *job.GetTarget())
        if (result.Diff >= job.GetTargetDiff())
        {
            result.Code = ShareCode::VALID_BLOCK;
            return result;
        }
        else if (result.Diff / cli.GetDifficulty() < 0.95)  // allow 5% below
        {
            result.Code = ShareCode::LOW_DIFFICULTY_SHARE;
            result.Message = "Low difficulty share";
            return result;
        }

        result.Code = ShareCode::VALID_SHARE;
        return result;
    }

    //    private:
    // std::vector<Job*> jobs;
};