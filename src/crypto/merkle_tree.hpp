
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
    template <typename TxRes>
    static std::vector<uint8_t> GetHashes(const std::vector<TxRes>& txsData)
    {
        std::vector<uint8_t> hashes;
        uint32_t hash_count = txsData.size();

        hashes.resize(txsData.size() * HASH_SIZE);

        for (int i = 0; i < hash_count; i++)
        {
            std::array<uint8_t, HASH_SIZE> hash_bin = Unhexlify<HASH_SIZE * 2>(txsData[i].hash);
            std::ranges::copy(hash_bin,
                      hashes.data() + i * HASH_SIZE);
        }

        return hashes;
    }

    static std::array<uint8_t, HASH_SIZE> CalcRoot(std::vector<uint8_t>&& hashes)
    {
        std::size_t hash_count = hashes.size() / HASH_SIZE;
        std::array<uint8_t, HASH_SIZE> res;
        while (hash_count > 1)
        {
            // if odd amount of leafs, duplicate the last
            if (hash_count & 1)
            {
                if (hashes.capacity() < (hash_count + 1) * HASH_SIZE)
                {
                    hashes.reserve(hashes.capacity() + HASH_SIZE);
                }
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

        memcpy(res.data(), hashes.data(), HASH_SIZE);
        return res;
    }

    // res needs to have coinbase txid, res needs to be HASH_SIZE
    static void CalcRootFromSteps(uint8_t* res, const uint8_t* cb_txid,
                                  const std::vector<uint8_t>& steps,
                                  const std::size_t step_count)
    {
        uint8_t buff[HASH_SIZE * 2];
        memcpy(buff, cb_txid, HASH_SIZE);
        for (int i = 0; i < step_count; i++)
        {
            memcpy(buff + HASH_SIZE, steps.data() + HASH_SIZE * i, HASH_SIZE);
            HashWrapper::SHA256d(buff, buff, HASH_SIZE * 2);
        }
        memcpy(res, buff, HASH_SIZE);
    }

    static int CalcSteps(std::vector<uint8_t>& res,
                         std::vector<uint8_t>& hashes, std::size_t hash_count)
    {
        int i = 0;
        while (hash_count > 1)
        {
            if (hash_count & 1)
            {
                // bug here because the new reseve doesnt contain the hashes (it
                // wasnt resized), so just make sure its big enough if
                // (hashes.capacity() < (hash_count + 1) * HASH_SIZE)
                // {
                //     hashes.reserve(hashes.capacity() + HASH_SIZE);
                // }
                memcpy(hashes.data() + hash_count * HASH_SIZE,
                       hashes.data() + (hash_count - 1) * HASH_SIZE, HASH_SIZE);
                hash_count++;
            }

            memcpy(res.data() + i * HASH_SIZE, hashes.data() + HASH_SIZE,
                   HASH_SIZE);

            // we can skip the first one as we won't use it (it's not even
            // known)
            for (int j = 2; j < hash_count; j += 2)
            {
                HashWrapper::SHA256d(hashes.data() + (j / 2) * HASH_SIZE,
                                     hashes.data() + j * HASH_SIZE,
                                     HASH_SIZE * 2);
            }

            hash_count = hash_count / 2;
            i++;
        }
        return i;
    }
};
#endif
