// Copyright (c) 2014-2018 Zano Project
// Copyright (c) 2014-2018 The Louisdor Project
// Copyright (c) 2012-2013 The Cryptonote developers
// Copyright (c) 2012-2013 The Boolberry developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "include_base_utils.h"
#include <boost/foreach.hpp>
#ifndef ANDROID_BUILD
  #include <boost/locale.hpp>
#endif
using namespace epee;

#include "print_fixed_point_helper.h"
#include "currency_format_utils.h"
#include "serialization/binary_utils.h"
#include "serialization/stl_containers.h"
#include "currency_core/currency_config.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "common/int-util.h"
#include "common/base58.h"

namespace currency
{

  //-----------------------------------------------------------------------
  bool is_coinbase(const transaction& tx)
  {
    if (!tx.vin.size() || tx.vin.size() > 2)
      return false;

    if (tx.vin[0].type() != typeid(txin_gen))
      return false;

    return true;
  }
  //-----------------------------------------------------------------------
  bool is_coinbase(const transaction& tx, bool& pos_coinbase)
  {
    if (!is_coinbase(tx))
      return false;

    pos_coinbase = (tx.vin.size() == 2 && tx.vin[1].type() == typeid(txin_to_key));
    return true;
  }


} // namespace currency


