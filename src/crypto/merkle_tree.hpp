
#ifndef MERKLE_TREE_HPP
#define MERKLE_TREE_HPP

#include <array>
#include <iostream>
#include <string>
#include <vector>
#include <experimental/array>

#include "utils.hpp"

class MerkleTree
{
   private:
    std::vector<unsigned char*> txIds;

   public:
    static void CalcRoot(std::vector<std::vector<unsigned char>>& txs,
                         unsigned char* res)
    {
        std::vector<std::array<unsigned char, 32>> txIds(txs.size());

        for (int i = 0; i < txs.size(); i++)
        {
#if COIN_CONFIG == COIN_VRSCTEST
            HashWrapper::SHA256d(txs[i].data(), txs[i].size(), txIds[i].data());
#endif
        }

        while (txIds.size() > 1)
        {
            std::vector<std::array<unsigned char, 32>> temp;
            for (int i = 0; i < txIds.size(); i += 2)
            {
                unsigned char combined[32 * 2];
                unsigned char combinedHash[32];

                if (i >= txIds.size())
                {
                    txIds.push_back(txIds.back());
                }

                memcpy(combined, txIds[i].data(), 32);
                memcpy(combined + 32, txIds[i + 1].data(), 32);
#if COIN_CONFIG == COIN_VRSCTEST
                HashWrapper::SHA256d(combined, 64, combinedHash);
#endif

                temp.push_back(std::experimental::to_array(combinedHash));
            }
            txIds = temp;
        }

        memcpy(res, txIds[0].data(), 32);
    }
};
#endif
