
#ifndef MERKLE_TREE_HPP
#define MERKLE_TREE_HPP

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "hash_wrapper.hpp"
#include "jobs/block_template.hpp"
#include "utils.hpp"

template <size_t HASHSIZE>
class MerkleTree
{
    using HashT = std::array<uint8_t, HASHSIZE>;

   public:
    template <typename TxRes>
    static std::vector<HashT> GetHashes(const std::vector<TxRes>& txsData)
    {
        std::vector<HashT> hashes;
        uint32_t hash_count = txsData.size();

        hashes.reserve(txsData.size());

        for (const auto& tData : txsData)
        {
            HashT hash_bin = Unhexlify<HASHSIZE * 2>(tData.hash);
            std::ranges::reverse(hash_bin);
            // hashes are given in BE
            hashes.push_back(hash_bin);
        }

        return hashes;
    }

    // needs to be && so we can call from constructor
    static HashT CalcRoot(std::vector<HashT>&& h)
    {
        auto hashes = std::move(h);
        std::size_t hash_count = hashes.size();
        HashT res;

        while (hash_count > 1)
        {
            // if odd amount of leafs, duplicate the last
            if (hash_count & 1)
            {
                hashes.push_back(h.back());
                hash_count++;
            }

            for (int i = 0; i < hash_count; i += 2)
            {
                HashWrapper::SHA256d(
                    reinterpret_cast<uint8_t*>(hashes.data() + (i / 2)),
                    reinterpret_cast<uint8_t*>(hashes.data() + i),
                    HASHSIZE * 2);
            }
            hash_count /= 2;
        }

        return hashes[0];
    }

    // res needs to have coinbase txid, res needs to be HASH_SIZE
    static void CalcRootFromSteps(uint8_t* res, const uint8_t* cb_txid,
                                  const std::vector<HashT>& steps,
                                  const std::size_t step_count)
    {
        uint8_t buff[HASH_SIZE * 2];
        memcpy(buff, cb_txid, HASH_SIZE);
        for (int i = 0; i < step_count; i++)
        {
            memcpy(buff + HASH_SIZE,
                   reinterpret_cast<const void*>(steps.data() + i), HASH_SIZE);
            HashWrapper::SHA256d(buff, buff, HASH_SIZE * 2);
        }
        memcpy(res, buff, HASH_SIZE);
    }

    static std::vector<HashT> CalcSteps(std::vector<HashT>& hashes)
    {
        std::size_t hash_count = hashes.size();
        
        std::vector<HashT> res;
        res.reserve(hashes.size());

        while (hash_count > 1)
        {
            if (hash_count & 1)
            {
                hashes.push_back(hashes.back());
                hash_count++;
            }

            res.push_back(hashes[1]);

            // we can skip the first one as we won't use it (it's not even
            // known)
            for (int i = 2; i < hash_count; i += 2)
            {
                HashWrapper::SHA256d(
                    reinterpret_cast<uint8_t*>(hashes.data() + (i / 2)),
                    reinterpret_cast<uint8_t*>(hashes.data() + i),
                    HASHSIZE * 2);
            }

            hash_count = hash_count / 2;
        }
        return res;
    }
};
#endif
