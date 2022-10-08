// Copyright (c) 2014-2018 Zano Project
// Copyright (c) 2014-2018 The Louisdor Project
// Copyright (c) 2012-2013 The Cryptonote developers
// Copyright (c) 2012-2013 The Boolberry developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <typeindex>
#include <unordered_set>
#include <unordered_map>

#include "include_base_utils.h"

#include "currency_format_utils_abstract.h"
#include "common/crypto_stream_operators.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "currency_format_utils_blocks.h"
#include "currency_format_utils_transactions.h"


// ------ get_tx_type_definition -------------
#define       GUI_TX_TYPE_NORMAL                  0
#define       GUI_TX_TYPE_PUSH_OFFER              1
#define       GUI_TX_TYPE_UPDATE_OFFER            2
#define       GUI_TX_TYPE_CANCEL_OFFER            3
#define       GUI_TX_TYPE_NEW_ALIAS               4
#define       GUI_TX_TYPE_UPDATE_ALIAS            5
#define       GUI_TX_TYPE_COIN_BASE               6
#define       GUI_TX_TYPE_ESCROW_PROPOSAL         7
#define       GUI_TX_TYPE_ESCROW_TRANSFER         8
#define       GUI_TX_TYPE_ESCROW_RELEASE_NORMAL   9
#define       GUI_TX_TYPE_ESCROW_RELEASE_BURN     10
#define       GUI_TX_TYPE_ESCROW_CANCEL_PROPOSAL  11
#define       GUI_TX_TYPE_ESCROW_RELEASE_CANCEL   12
#define       GUI_TX_TYPE_HTLC_DEPOSIT            13
#define       GUI_TX_TYPE_HTLC_REDEEM             14




namespace currency
{
  bool is_coinbase(const transaction& tx);

  inline const std::vector<txin_etc_details_v>* get_input_etc_details(
      const txin_v& in)
  {
      if (in.type().hash_code() == typeid(txin_to_key).hash_code())
          return &boost::get<txin_to_key>(in).etc_details;
      if (in.type().hash_code() == typeid(txin_htlc).hash_code())
          return &boost::get<txin_htlc>(in).etc_details;
      if (in.type().hash_code() == typeid(txin_multisig).hash_code())
          return &boost::get<txin_multisig>(in).etc_details;
      return nullptr;
  }
  //---------------------------------------------------------------
  inline size_t get_input_expected_signatures_count(const txin_v& tx_in)
  {
    struct txin_signature_size_visitor : public boost::static_visitor<size_t>
    {
      size_t operator()(const txin_gen& /*txin*/) const   { return 0; }
      size_t operator()(const txin_to_key& txin) const    { return txin.key_offsets.size(); }
      size_t operator()(const txin_multisig& txin) const  { return txin.sigs_count; }
      size_t operator()(const txin_htlc& txin) const      { return 1; }
    };

    return boost::apply_visitor(txin_signature_size_visitor(), tx_in);
  }
  //---------------------------------------------------------------

} // namespace currency
