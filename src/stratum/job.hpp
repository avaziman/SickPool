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

    // virtual uint8_t* GetHeaderData(uint8_t* buff, std::string_view time,
    //                                      std::string_view nonce1,
    //                                      std::string_view nonce2,
    //                                      std::string_view additional) = 0;

    uint8_t* GetStaticHeaderData() { return this->staticHeaderData; }

    void GetBlockHex(uint8_t* header,char* res)
    {
        Hexlify(res, header, BLOCK_HEADER_SIZE);
        memcpy(res + (BLOCK_HEADER_SIZE * 2), txsHex.data(), txsHex.size());
    }

    int64_t GetBlockReward()
    {
        // return transactions[0]->GetOutputs()->at(0).value;
        return blockReward;
    }
    // char* GetVersion() { return version; }
    // char* GetPrevBlockhash() { return hashPrevBlock; }
    // char* GetTime() { return nTime; }
    // char* GetBits() { return nBits; }
    uint8_t* GetPrevBlockHash() { return staticHeaderData + 4; } //TODO: make VERSION_SIZE
    uint32_t GetHeight() { return height; }
    int GetTransactionCount() { return txCount; }
    int GetBlockSize() { return (BLOCK_HEADER_SIZE * 2) + txsHex.size(); }
    const char* GetId() { return jobIdStr; }
    int64_t GetMinTime() { return minTime; }
    char* GetNotifyBuff() { return notifyBuff; }
    std::size_t GetNotifyBuffSize() { return notifyBuffSize; }
    double GetTargetDiff() { return targetDiff; }
    // arith_uint256* GetTarget() { return &target; }

   protected:
    // arith_uint256 target;
    std::vector<char> txsHex;
    uint64_t txAmountByteValue;
    int txAmountByteLength;
    int txCount;

    const uint32_t jobId;
    const int64_t minTime;
    double targetDiff;
    const int64_t blockReward;
    const uint32_t height;

    // unsigned char headerData[BLOCK_HEADER_SIZE];
    uint8_t staticHeaderData[BLOCK_HEADER_STATIC_SIZE];
    char jobIdStr[8 + 1];

    char notifyBuff[MAX_NOTIFY_MESSAGE_SIZE];
    std::size_t notifyBuffSize;
    //  char version[4];
    //  char hashPrevBlock[32];
    //  char hashMerkleRoot[32];
    //  char nTime[4];
    //  char nBits[4];
};
#endif