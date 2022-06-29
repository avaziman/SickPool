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
    Job(uint32_t jobId, const BlockTemplate& bTemplate)
        : jobId(jobId),
          blockReward(bTemplate.coinbaseValue),
          minTime(bTemplate.minTime),
          height(bTemplate.height)
    //   target()
    {
        // target.SetHex(std::string(bTemplate.target));

        ToHex(jobIdStr, jobId);

        txCount = bTemplate.txList.transactions.size();
        txAmountByteValue = txCount;

        txAmountByteLength = VarInt(txAmountByteValue);
        txsHex = std::vector<char>(
            (txAmountByteLength + bTemplate.txList.byteCount) * 2);
        Hexlify(txsHex.data(), (unsigned char*)&txAmountByteValue,
                txAmountByteLength);

        int written = txAmountByteLength * 2;
        for (int i = 0; i < bTemplate.txList.transactions.size(); i++)
        {
            memcpy(txsHex.data() + written,
                   bTemplate.txList.transactions[i].dataHex.data(),
                   bTemplate.txList.transactions[i].dataHex.size());
            written += bTemplate.txList.transactions[i].dataHex.size();
        }

        // Logger::Log(LogType::Debug, LogField::JobManager, "tx hex: %.*s",
        //             txsHex.size(), txsHex.data());
    }

    uint8_t* GetStaticHeaderData() { return this->staticHeaderData; }

    void GetBlockHex(const uint8_t* header, char* res) const
    {
        Hexlify(res, header, BLOCK_HEADER_SIZE);
        memcpy(res + (BLOCK_HEADER_SIZE * 2), txsHex.data(), txsHex.size());
    }

    int64_t GetBlockReward() const { return blockReward; }
    // char* GetVersion() { return version; }
    // char* GetPrevBlockhash() { return hashPrevBlock; }
    // char* GetTime() { return nTime; }
    // char* GetBits() { return nBits; }
    // TODO: make VERSION_SIZE
    uint8_t* GetPrevBlockHash() { return staticHeaderData + 4; }
    uint32_t GetHeight() const { return height; }
    int GetTransactionCount() const { return txCount; }
    int GetBlockSize() const
    {
        return (BLOCK_HEADER_SIZE * 2) + (int)txsHex.size();
    }
    std::string_view GetId() const { return std::string_view(jobIdStr, sizeof(jobIdStr)); }
    int64_t GetMinTime() const { return minTime; }
    char* GetNotifyBuff() { return notifyBuff; }
    std::size_t GetNotifyBuffSize() const { return notifyBuffSize; }
    double GetTargetDiff() const { return targetDiff; }
    double GetEstimatedShares() const { return expectedShares; }
    // arith_uint256* GetTarget() { return &target; }

   protected:
    const uint32_t jobId;
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
    int txAmountByteLength;
    int txCount;

    const int64_t minTime;
    const uint32_t height;

    char jobIdStr[8];
};
#endif