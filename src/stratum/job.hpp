#ifndef JOB_HPP_
#define JOB_HPP_
#include <iomanip>
#include <sstream>
#include <string>

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
        // only generating notify message once for efficiency
        // we need to have all values in bytes, not

        ToHex(jobIdStr, jobId);
        jobIdStr[8] = 0;
    }

    virtual unsigned char* GetData(const char* time, const char* nonce1, const char* nonce2, const char* additional, int solSize) = 0;
    void GetHash(unsigned char* res)
    {
#if POOL_COIN == COIN_VRSCTEST
        HashWrapper::VerushashV2b2(this->blockData, BLOCK_HEADER_SIZE, res);
#endif
    }
    // char* GetVersion() { return version; }
    // char* GetPrevBlockhash() { return hashPrevBlock; }
    // char* GetTime() { return nTime; }
    // char* GetBits() { return nBits; }
    const char* GetId() { return jobIdStr; }
    char* GetNotifyBuff() { return notifyBuff; }
    uint16_t GetNotifyBuffSize() { return notifyBuffSize; }
    double GetTargetDiff() { return targetDiff; }

   protected:
    MerkleTree merkleTree;
    uint32_t jobId;
    char jobIdStr[9];
    unsigned char blockData[BLOCK_HEADER_SIZE];
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