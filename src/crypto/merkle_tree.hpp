
#ifndef MERKLE_TREE_HPP
#define MERKLE_TREE_HPP

#include <array>
#include <iostream>
#include <string>
#include <vector>
#include <experimental/array>
#include "./hash_wrapper.hpp"
#include "../stratum/block_template.hpp"

#include "utils.hpp"

class MerkleTree
{
   public:
    static void CalcRoot(std::vector<TransactionData>& txsData, unsigned char* res)
    {
        std::vector<std::array<unsigned char, 32>> hashes(txsData.size());
        for (int i = 0; i < hashes.size(); i++){
            hashes[i] = std::experimental::to_array(txsData[i].hash);
        }

        while (hashes.size() > 1)
        {
            std::vector<std::array<unsigned char, 32>> temp;

            for (int i = 0; i < hashes.size(); i += 2)
            {
                unsigned char combined[32 * 2];
                unsigned char combinedHash[32];

                if (i >= hashes.size())
                {
                    hashes.push_back(hashes.back());
                }

                memcpy(combined, hashes[i].data(), 32);
                memcpy(combined + 32, hashes[i + 1].data(), 32);
                HashWrapper::SHA256d(combinedHash, combined, 64);

                temp.push_back(std::experimental::to_array(combinedHash));
            }
            hashes = temp;
        }

        memcpy(res, hashes[0].data(), 32);
    }
};
#endif
