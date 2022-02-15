
#ifndef MERKLE_TREE_HPP
#define MERKLE_TREE_HPP

#include <iostream>
#include <string>
#include <vector>
#include "utils.hpp"

class MerkleTree
{
   private:
    std::vector<char*> txIds;
    void(*hash)(char*, int, char*);

   public:
    MerkleTree()
        : hash(hash)
    {
    }

    void AddTx(char* txHex, int size) {
        char txId[32];
        hash(txHex, size, txId);
        txIds.push_back(txId);
    }

    void CalcRoot(char* res)
    {
        std::vector<char*> leafs(txIds);

        while (leafs.size() != 1)
        {
            if (leafs.size() % 2 != 0) leafs.push_back(leafs[leafs.size() - 1]);

            std::vector<char*> tempLeafs;

            for (int i = 0; i < leafs.size(); i += 2)
            {
                // std::string combined = (leafs[i]) + (leafs[i + 1]);
                char combinedHash[32];
                hash(leafs[i], 32 * 2, combinedHash);
                tempLeafs.push_back(combinedHash);
            }

            leafs = tempLeafs;
        }
        std::memcpy(res, leafs[0], 32);
    } 
};
#endif