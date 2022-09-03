
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
    // static void CalcRoot(
    //     uint8_t* res, const std::vector<TransactionData>& txsData)
    // {
    //     std::vector<uint8_t> hashes;
    //     uint32_t hash_count = txsData.size();

    //     hashes.reserve(txsData.size() * HASH_SIZE);

    //     for (int i = 0; i < hash_count; i++)
    //     {
    //         memcpy(hashes.data() + i * HASH_SIZE, txsData[i].hash, HASH_SIZE);
    //     }

    //     CalcRoot(res, hashes, hash_count);
    // }

    static void CalcRoot(uint8_t* res, std::vector<uint8_t>& hashes,
                         std::size_t hash_count)
    {
        while (hash_count > 1)
        {
            for (int i = 0; i < hash_count; i += 2)
            {
                if (i + 1 == hash_count)
                {
                    hashes.reserve(hashes.size() + i * HASH_SIZE);
                    memcpy(hashes.data() + i * HASH_SIZE,
                           hashes.data() + (i - 1) * HASH_SIZE, HASH_SIZE);
                }

                HashWrapper::SHA256d(hashes.data() + (i / 2) * HASH_SIZE,
                                     hashes.data() + i * HASH_SIZE,
                                     HASH_SIZE * 2);
            }
            hash_count /= 2;
        }

        memcpy(res, hashes.data(), HASH_SIZE);
    }
};
#endif
