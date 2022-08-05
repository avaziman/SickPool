
#ifndef MERKLE_TREE_HPP
#define MERKLE_TREE_HPP

#include <array>
#include <iostream>
#include <string>
#include <vector>
#include "hash_wrapper.hpp"
#include "jobs/block_template.hpp"

#include "utils.hpp"

class MerkleTree
{
   public:
    static void CalcRoot(
        uint8_t* res, const std::vector<TransactionData>& txsData)
    {
        std::vector<std::array<uint8_t, HASH_SIZE>> hashes(txsData.size());
        for (int i = 0; i < hashes.size(); i++){
            hashes[i] = std::to_array(txsData[i].hash);
        }

        while (hashes.size() > 1)
        {
            std::vector<std::array<unsigned char, HASH_SIZE>> temp;

            for (int i = 0; i < hashes.size(); i += 2)
            {
                uint8_t combined[HASH_SIZE * 2];
                uint8_t combinedHash[HASH_SIZE];

                if (i >= hashes.size())
                {
                    hashes.push_back(hashes.back());
                }

                memcpy(combined, hashes[i].data(), HASH_SIZE);
                memcpy(combined + HASH_SIZE, hashes[i + 1].data(), HASH_SIZE);
                HashWrapper::SHA256d(combinedHash, combined, sizeof(combined));

                temp.push_back(std::to_array(combinedHash));
            }
            hashes = temp;
        }

        memcpy(res, hashes[0].data(), HASH_SIZE);
    }
};
#endif
