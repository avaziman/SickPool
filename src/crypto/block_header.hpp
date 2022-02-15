#ifndef BLOCK_HEADER_HPP_
#define BLOCK_HEADER_HPP_
#include <iomanip>
#include <sstream>
#include <string>

#include "utils.hpp"

class BlockHeader
{
   public:
    BlockHeader(char* ver, char* prevBlock, char* time,
                char* bits) 
    {
        memcpy(this->version, ver, 8 + 1);
        memcpy(this->hashPrevBlock, prevBlock, 64 + 1);
        memcpy(this->nTime, time, 8 + 1);
        memcpy(this->nBits, bits, 8 + 1);
    }

    virtual char* GetHex(char* time, char* merkleRootHash,
                               char* nonce) = 0;
    char* GetVersion() { return version; }
    char* GetPrevBlockhash() { return hashPrevBlock; }
    char* GetTime() { return nTime; }
    char* GetBits() { return nBits; }

   protected:
    char* hex;
    char version      [8  + 1];
    char hashPrevBlock[64 + 1];
    char nTime        [8  + 1];
    char nBits        [8  + 1];
};
#endif