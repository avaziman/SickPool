// Copyright (c) 2018-2019 Zano Project
// Copyright (c) 2018-2019 Hyle Team
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "include_base_utils.h"
using namespace epee;

#include "basic_pow_helpers.h"
#include "common/int-util.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "currency_core/currency_config.h"
#include "currency_format_utils_blocks.h"
#include "ethash/ethash.hpp"
#include "ethash/progpow.hpp"
#include "serialization/binary_utils.h"
#include "serialization/stl_containers.h"

namespace currency
{

//------------------------------------------------------------------
int ethash_height_to_epoch(uint64_t height)
{
    return static_cast<int>(height / ETHASH_EPOCH_LENGTH);
}
//--------------------------------------------------------------
crypto::hash ethash_epoch_to_seed(int epoch)
{
    auto res_eth = ethash_calculate_epoch_seed(epoch);
    crypto::hash result = currency::null_hash;
    memcpy(&result.data, &res_eth, sizeof(res_eth));
    return result;
}
//--------------------------------------------------------------
crypto::hash get_block_longhash(uint64_t height,
                                const crypto::hash& block_header_hash,
                                uint64_t nonce)
{
    int epoch = ethash_height_to_epoch(height);
    std::shared_ptr<ethash::epoch_context_full> p_context =
        progpow::get_global_epoch_context_full(static_cast<int>(epoch));
    CHECK_AND_ASSERT_THROW_MES(
        p_context, "progpow::get_global_epoch_context_full returned null");
    auto res_eth = progpow::hash(*p_context, static_cast<int>(height),
                                 *(ethash::hash256*)&block_header_hash, nonce);
    crypto::hash result = currency::null_hash;
    memcpy(&result.data, &res_eth.final_hash, sizeof(res_eth.final_hash));
    return result;
}

//TODO: make context for each worker count
void get_block_longhash_sick(uint8_t* res, const uint64_t height,
                             const uint8_t* block_header_hash,
                             const uint64_t nonce)
{
    int epoch = ethash_height_to_epoch(height);
    std::shared_ptr<ethash::epoch_context_full> p_context =
        progpow::get_global_epoch_context_full(static_cast<int>(epoch));
    CHECK_AND_ASSERT_THROW_MES(
        p_context, "progpow::get_global_epoch_context_full returned null");
    auto res_eth = progpow::hash(*p_context, static_cast<int>(height),
                                 *reinterpret_cast<const ethash::hash256*>(block_header_hash), nonce);
    memcpy(res, &res_eth.final_hash, sizeof(res_eth.final_hash));
}
//---------------------------------------------------------------
crypto::hash get_block_header_mining_hash(const block& b)
{
    blobdata bd = get_block_hashing_blob(b);

    access_nonce_in_block_blob(bd) = 0;
    return crypto::cn_fast_hash(bd.data(), bd.size());
}

}  // namespace currency