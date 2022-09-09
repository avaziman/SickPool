
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
            // if odd amount of leafs, duplicate the last
            if (hash_count & 1)
            {
                hashes.reserve(hashes.size() + HASH_SIZE);
                memcpy(hashes.data() + hash_count * HASH_SIZE,
                       hashes.data() + (hash_count - 1) * HASH_SIZE, HASH_SIZE);
                hash_count++;
            }

            for (int i = 0; i < hash_count; i += 2)
            {

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
