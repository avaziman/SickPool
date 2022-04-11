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
    Job(uint32_t jobId, int64_t blockReward, uint32_t height, TransactionDataList& txs,
        uint32_t minTime)
        : jobId(jobId),
          blockReward(blockReward),
          minTime(minTime),  //, time(std::time(0))
          height(height)
    {
        ToHex(jobIdStr, jobId);

        txAmountByteValue = txCount = txs.transactions.size();
        txAmountByteLength = VarInt(txAmountByteValue);
        txsHex = std::vector<char>((txAmountByteLength + txs.byteCount) * 2);
        Hexlify(txsHex.data(), (unsigned char*)&txAmountByteValue, txAmountByteLength);

        int written = txAmountByteLength * 2;
        for (int i = 0; i < txs.transactions.size(); i++)
        {
            int txSize = txs.transactions[i].dataHex.size();
            std::cout << "f: " << txs.transactions[i].dataHex << std::endl;
            memcpy(txsHex.data() + written,
                   txs.transactions[i].dataHex.data(),
                   txs.transactions[i].dataHex.size());
            written += txs.transactions[i].dataHex.size();
        }

        std::cout << "tx hex: " << txsHex.data() << std::endl;
    }

    virtual unsigned char* GetHeaderData(std::string_view time,
                                         std::string_view nonce1,
                                         std::string_view nonce2,
                                         std::string_view additional) = 0;

    unsigned char* GetHeaderData() { return this->headerData; }

    void GetBlockHex(char* res)
    {
        Hexlify(res, headerData, BLOCK_HEADER_SIZE);
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
    unsigned char* GetPrevBlockHash() { return headerData + 4; }
    uint32_t GetHeight() { return height; }
    int GetTransactionCount() { return txCount; }
    int GetBlockSize() { return (BLOCK_HEADER_SIZE * 2) + txsHex.size(); }
    const char* GetId() { return jobIdStr; }
    std::time_t GetMinTime() { return minTime; }
    char* GetNotifyBuff() { return notifyBuff; }
    std::size_t GetNotifyBuffSize() { return notifyBuffSize; }
    double GetTargetDiff() { return targetDiff; }

   protected:
    std::vector<char> txsHex;
    uint64_t txAmountByteValue;
    int txAmountByteLength;
    int txCount;

    const uint32_t jobId;
    const std::time_t minTime;
    double targetDiff;
    const int64_t blockReward;
    const uint32_t height;

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