#ifndef JOB_HPP_
#define JOB_HPP_
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "../crypto/merkle_tree.hpp"
#include "../crypto/utils.hpp"

#if POOL_COIN == COIN_VRSCTEST
#define BLOCK_HEADER_SIZE 140 + 3 + 1344
#define MAX_NOTIFY_MESSAGE_SIZE 512
// with true 444
#endif

class Job
{
   public:
    Job(uint32_t jobId, std::vector<std::vector<unsigned char>>& txs) /*, char*
        ver, char* prevBlock, char* time, char* bits)*/
        : jobId(jobId)
    {
        ToHex(jobIdStr, jobId);
        jobIdStr[8] = 0;

        uint64_t varIntVal = txs.size();
        int varIntLen = VarInt(varIntVal);
        int txDataLen = 0;

        for (int i = 0; i < txs.size(); i++) txDataLen += txs[i].size();
        
        //TODO: fix sizes
        // insert the txdata as hex as we will need to submit it as hex anyway
        txDataHex.resize((txDataLen + varIntLen) * 2);

        unsigned char varInt[8];
        memcpy(varInt, &varIntVal, varIntLen);
        Hexlify(varInt, varIntLen, txDataHex.data());

        int written = varIntLen * 2;

        for (int i = 0; i < txs.size(); i++)
        {
            Hexlify(txs[i].data(), txs[i].size(), txDataHex.data() + written);
            written += txs[i].size() * 2;
        }
    }

    virtual unsigned char* GetHeaderData(const char* time, const char* nonce1,
                                         const char* nonce2,
                                         const char* additional,
                                         int solSize) = 0;

    void GetBlockHex(char* res)
    {
        Hexlify(headerData, BLOCK_HEADER_SIZE, res);
        // TODO: CHECK WHY BLOCK_HEADER_SIZE * 2 WON"T WORK (SKIPS 72 bytes)
        memcpy(res + (2974), txDataHex.data(), txDataHex.size());
    }

    void GetHash(unsigned char* res)
    {
#if POOL_COIN == COIN_VRSCTEST
        HashWrapper::VerushashV2b2(this->headerData, BLOCK_HEADER_SIZE, res);
#endif
    }
    // char* GetVersion() { return version; }
    // char* GetPrevBlockhash() { return hashPrevBlock; }
    // char* GetTime() { return nTime; }
    // char* GetBits() { return nBits; }
    const int GetBlockSize() { return (BLOCK_HEADER_SIZE * 2) + txDataHex.size(); }
    const char* GetId() { return jobIdStr; }
    char* GetNotifyBuff() { return notifyBuff; }
    uint16_t GetNotifyBuffSize() { return notifyBuffSize; }
    double GetTargetDiff() { return targetDiff; }

   protected:
    unsigned char headerData[BLOCK_HEADER_SIZE];
    std::vector<char> txDataHex;
    uint32_t jobId;
    char jobIdStr[9];
    uint16_t notifyBuffSize;
    char notifyBuff[MAX_NOTIFY_MESSAGE_SIZE];
    double targetDiff;
    // char version[4];
    // char hashPrevBlock[32];
    // char hashMerkleRoot[32];
    // char nTime[4];
    // char nBits[4];
};
#endif