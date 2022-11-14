// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>
#include "uint256.h"
#include "util/strencodings.h"

template <unsigned int BITS>
base_blob<BITS>::base_blob(const std::vector<unsigned char>& vch)
{
    assert(vch.size() == sizeof(m_data));
    memcpy(m_data, vch.data(), sizeof(m_data));
}

template <unsigned int BITS>
std::string base_blob<BITS>::GetHex() const
{
    uint8_t m_data_rev[WIDTH];
    for (int i = 0; i < WIDTH; ++i)
    {
        m_data_rev[i] = m_data[WIDTH - 1 - i];
    }
    return HexStr(m_data_rev);
}

// template <unsigned int BITS>

template <unsigned int BITS>
void base_blob<BITS>::SetHex(const std::string& str)
{
    SetHex(str.c_str());
}

template <unsigned int BITS>
std::string base_blob<BITS>::ToString() const
{
    return (GetHex());
}

// Explicit instantiations for base_blob<160>
template base_blob<160>::base_blob(const std::vector<unsigned char>&);
template std::string base_blob<160>::GetHex() const;
template std::string base_blob<160>::ToString() const;
template void base_blob<160>::SetHex(const char*);
template void base_blob<160>::SetHex(const std::string&);

// Explicit instantiations for base_blob<256>
template base_blob<256>::base_blob(const std::vector<unsigned char>&);
template std::string base_blob<256>::GetHex() const;
template std::string base_blob<256>::ToString() const;
template void base_blob<256>::SetHex(const char*);
template void base_blob<256>::SetHex(const std::string&);

const uint256 uint256::ZERO(0);
const uint256 uint256::ONE(1);
// constinit uint256 TWO = uint256S("2");