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

#if POOL_COIN == COIN_VRSCTEST
#define MAX_NOTIFY_MESSAGE_SIZE (1024 * 4)
// with true 444
#endif

class Job
{
   public:
    Job(uint32_t jobId, int64_t blockReward, TransactionDataList& txs,
        uint32_t minTime)
        : jobId(jobId),
          blockReward(blockReward),
          minTime(minTime)  //, time(std::time(0))
    {
        ToHex(jobIdStr, jobId);

        txAmountByteValue = txs.transactions.size();
        txAmountByteLength = VarInt(txAmountByteValue);
        txsHex = std::vector<unsigned char>(txAmountByteLength + txs.byteCount);
        memcpy(txsHex.data(), &txAmountByteValue, txAmountByteLength);

        int written = 0;
        for (int i = 0; i < txs.transactions.size(); i++)
        {
            memcpy(txsHex.data() + txAmountByteLength + written,
                   txs.transactions[i].dataHex.data(),
                   txs.transactions[i].dataHex.size());
            written += txs.transactions[i].dataHex.size();
        }
    }

    virtual unsigned char* GetHeaderData(std::string_view time,
                                         std::string_view nonce1,
                                         std::string_view nonce2,
                                         std::string_view additional) = 0;

    unsigned char* GetHeaderData() { return this->headerData; }

    void GetBlockHex(char* res)
    {
        Hexlify(res, headerData, BLOCK_HEADER_SIZE);
        Hexlify(res + (BLOCK_HEADER_SIZE * 2),
                (unsigned char*)txAmountByteValue, txAmountByteLength);

        // for (int i = 0; i < txsBytes.size(); i++)
        // {
        //     Hexlify(res + (BLOCK_HEADER_SIZE * 2) + txAmountByteLength +
        //                 (i * (txsBytes[i].size() * 2)),
        //             txsBytes[i].data(), txsBytes[i].size());
        // }
        // memcpy(res + (BLOCK_HEADER_SIZE * 2), txDataHex.data(),
        //        txDataHex.size());
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
    const int GetBlockSize() { return (BLOCK_HEADER_SIZE * 2) + txsHex.size(); }
    const char* GetId() { return jobIdStr; }
    std::time_t GetMinTime() { return minTime; }
    char* GetNotifyBuff() { return notifyBuff; }
    std::size_t GetNotifyBuffSize() { return notifyBuffSize; }
    double GetTargetDiff() { return targetDiff; }

   protected:
    std::vector<unsigned char> txsHex;
    uint64_t txAmountByteValue;
    int txAmountByteLength;

    const uint32_t jobId;
    const std::time_t minTime;
    double targetDiff;
    const int64_t blockReward;

    unsigned char headerData[BLOCK_HEADER_SIZE];
    char jobIdStr[8 + 1];

    char notifyBuff[MAX_NOTIFY_MESSAGE_SIZE];
    std::size_t notifyBuffSize;
    // std::time_t created;
    //  char version[4];
    //  char hashPrevBlock[32];
    //  char hashMerkleRoot[32];
    //  char nTime[4];
    //  char nBits[4];
};
#endif