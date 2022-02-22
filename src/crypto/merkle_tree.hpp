
#ifndef MERKLE_TREE_HPP
#define MERKLE_TREE_HPP

#include <iostream>
#include <string>
#include <vector>

#include "utils.hpp"

class MerkleTree
{
   private:
    std::vector<unsigned char*> txIds;

   public:
    MerkleTree(std::vector<std::string> txs)
    {
        for (int i = 0; i < txs.size(); i++) AddTx(txs[i].c_str(), txs[i].size());
    }

    void AddTx(const char* txHex, int size)
    {
        unsigned char txId[32];
        // TODO: unhexlify
#if COIN_CONFIG == COIN_VRSC
        HashWrapper::SHA256d((unsigned char*)txHex, size, txId);
#endif
        txIds.push_back(txId);
    }

    void CalcRoot(unsigned char* res)
    {
        std::vector<unsigned char*> leafs(txIds);

        while (leafs.size() != 1)
        {
            if (leafs.size() % 2 != 0) leafs.push_back(leafs[leafs.size() - 1]);

            std::vector<unsigned char*> tempLeafs;

            for (int i = 0; i < leafs.size(); i += 2)
            {
                // std::string combined = (leafs[i]) + (leafs[i + 1]);
                unsigned char combinedHash[32];
#if COIN_CONFIG == COIN_VRSC
                HashWrapper::SHA256d(leafs[i], 32 * 2, combinedHash);
#endif
                tempLeafs.push_back(combinedHash);
            }

            leafs = tempLeafs;
        }
        std::memcpy(res, leafs[0], 32);
    }
};
#endif