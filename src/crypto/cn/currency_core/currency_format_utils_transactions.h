// Copyright (c) 2018-2019 Zano Project
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once


#include "include_base_utils.h"
#include "crypto/crypto.h"
#include "currency_core/currency_basic.h"
#include "currency_protocol/blobdatatype.h"


namespace currency
{
template <class extra_type_t>
uint64_t get_tx_x_detail(const transaction& tx)
{
    extra_type_t e = AUTO_VAL_INIT(e);
    get_type_in_variant_container(tx.extra, e);
    return e.v;
}

size_t get_object_blobsize(const transaction& t);
size_t get_objects_blobsize(const std::list<transaction>& ls);
size_t get_object_blobsize(const transaction& t, uint64_t prefix_blob_size);
inline uint64_t get_tx_flags(const transaction& tx) { return get_tx_x_detail<etc_tx_details_flags>(tx); }

crypto::hash get_transaction_hash(const transaction& t);
bool get_transaction_hash(const transaction& t, crypto::hash& res);
bool get_transaction_hash(const transaction& t, crypto::hash& res,
                          uint64_t& blob_size);
}
