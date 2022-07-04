#ifndef JOB_HPP_
#define JOB_HPP_
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "../crypto/merkle_tree.hpp"
#include "../crypto/utils.hpp"
#include "block_template.hpp"

#define EXTRANONCE_SIZE 4
#define VERSION_SIZE 4
#define TIME_SIZE 4
#define BITS_SIZE 4
#define PREVHASH_SIZE HASH_SIZE
#define MERKLE_ROOT_SIZE HASH_SIZE
#define FINALSROOT_SIZE HASH_SIZE
#define NONCE_SIZE HASH_SIZE
#define SOLUTION_SIZE 1344
#define SOLUTION_LENGTH_SIZE 3

#define BLOCK_HEADER_STATIC_SIZE                                  \
    VERSION_SIZE           /* version */                          \
        + PREVHASH_SIZE    /* prevhash */                         \
        + MERKLE_ROOT_SIZE /* merkle_root */                      \
        + FINALSROOT_SIZE  /* final sapling root */               \
        + TIME_SIZE        /* time, not static but we override */ \
        + BITS_SIZE        /* bits */

class Job
{
   public:
    Job(const std::string& jobId, const BlockTemplate& bTemplate)
        : jobId(jobId),
          blockReward(bTemplate.coinbaseValue),
          minTime(bTemplate.minTime),
          height(bTemplate.height)
    //   target()
    {
        // target.SetHex(std::string(bTemplate.target));

        txCount = bTemplate.txList.transactions.size();
        txAmountByteValue = txCount;

        txAmountByteLength = VarInt(txAmountByteValue);
        txsHex = std::vector<char>(
            (txAmountByteLength + bTemplate.txList.byteCount) * 2);
        Hexlify(txsHex.data(), (unsigned char*)&txAmountByteValue,
                txAmountByteLength);

        std::size_t written = txAmountByteLength * 2;
        for (const auto& txData : bTemplate.txList.transactions)
        {
            memcpy(txsHex.data() + written, txData.dataHex.data(),
                   txData.dataHex.size());
            written += txData.dataHex.size();
        }

        // Logger::Log(LogType::Debug, LogField::JobManager, "tx hex: %.*s",
        //             txsHex.size(), txsHex.data());
    }

    uint8_t* GetStaticHeaderData() { return this->staticHeaderData; }

    inline void GetBlockHex(const uint8_t* header, char* res) const
    {
        Hexlify(res, header, BLOCK_HEADER_SIZE);
        memcpy(res + (BLOCK_HEADER_SIZE * 2), txsHex.data(), txsHex.size());
    }

    int64_t GetBlockReward() const { return blockReward; }
    // char* GetVersion() { return version; }
    // char* GetPrevBlockhash() { return hashPrevBlock; }
    // char* GetTime() { return nTime; }
    // char* GetBits() { return nBits; }
    uint8_t* GetPrevBlockHash() { return staticHeaderData + VERSION_SIZE; }
    uint32_t GetHeight() const { return height; }
    int GetTransactionCount() const { return txCount; }
    std::size_t GetBlockSize() const
    {
        return (BLOCK_HEADER_SIZE * 2) + (int)txsHex.size();
    }
    std::string_view GetId() const { return std::string_view(jobId); }
    int64_t GetMinTime() const { return minTime; }
    const char* GetNotifyBuff() const { return notifyBuff; }
    std::size_t GetNotifyBuffSize() const { return notifyBuffSize; }
    double GetTargetDiff() const { return targetDiff; }
    double GetEstimatedShares() const { return expectedShares; }
    // arith_uint256* GetTarget() { return &target; }

   protected:
    const std::string jobId;
    uint8_t staticHeaderData[BLOCK_HEADER_STATIC_SIZE];
    char notifyBuff[MAX_NOTIFY_MESSAGE_SIZE];
    std::size_t notifyBuffSize;
    const int64_t blockReward;
    /*const*/ double targetDiff = 0;
    /*const*/ double expectedShares = 0;

   private:
    // arith_uint256 target;
    std::vector<char> txsHex;
    uint64_t txAmountByteValue;
    std::size_t txAmountByteLength;
    std::size_t txCount;

    const int64_t minTime;
    const uint32_t height;
};
#endif