#ifndef HASH_WRAPPER_HPP
#define HASH_WRAPPER_HPP

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "static_config.hpp"
#include "verushash/uint256.h"
#include "verushash/sha256.h"
#include "verushash/verus_clhash.h"
#include "verushash/verus_hash.h"
#include "x25x/x25x.h"

class HashWrapper
{
   public:
    static void InitVerusHash();
    static void InitSHA256();
    inline static void VerushashV2b2(unsigned char* dest,
                                     const unsigned char* in, int size,
                                     CVerusHashV2* hasher)
    {
        hasher->Reset();
        hasher->Write(in, size);
        hasher->Finalize2b(dest);
    }
    inline static void SHA256d(unsigned char* dest, const unsigned char* in,
                               int size)
    {
        // Make sure its initialized before this

        unsigned char hash1[HASH_SIZE];

        csha256.Reset();
        csha256.Write(in, size);
        csha256.Finalize(hash1);

        csha256.Reset();

        csha256.Write(hash1, sizeof(hash1));
        csha256.Finalize(dest);
    }

    inline static void X25X(unsigned char* dest, const unsigned char* in,
                            int size = 80)
    {
        x25x_hash(dest, in, size);
    }

    inline static void X22I(unsigned char* dest, const unsigned char* in,
                            int size = 80)
    {
        x22i_hash(dest, in);
    }

   private:
    static CSHA256 csha256;
};
#endif