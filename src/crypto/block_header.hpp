#ifndef BLOCK_HEADER_HPP_
#define BLOCK_HEADER_HPP_
#include <iomanip>
#include <sstream>
#include <string>

#include "utils.hpp"

class BlockHeader
{
   public:
    BlockHeader(uint32_t ver, std::string prevBlock, uint32_t time,
                std::string bits)
        : version(ver), hashPrevBlock(prevBlock), nTime(time), nBits(bits)
    {
    }

    virtual std::string GetHex(std::string time, std::string merkleRootHash,
                               std::string nonce) = 0;
    std::string GetVersion()
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8) << htobe32(version);
        return ss.str();
    }
    std::string GetPrevBlockhash() { return hashPrevBlock; }
    std::string GetTime()
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8) << htobe32(nTime);
        return ss.str();
    }
    std::string GetBits() { return nBits; }

   protected:
    uint32_t version;
    std::string hashPrevBlock;
    // std::string hashMerkleRoot;
    uint32_t nTime;
    std::string nBits;
};
#endif