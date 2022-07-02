#ifndef HASH_WRAPPER_HPP
#define HASH_WRAPPER_HPP

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "verushash/uint256.h"
#include "verushash/sha256.h"
#include "verushash/verus_clhash.h"
#include "verushash/verus_hash.h"

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

        unsigned char hash1[32];

        csha256.Reset();
        csha256.Write(in, size);
        csha256.Finalize(hash1);

        csha256.Reset();

        csha256.Write(hash1, sizeof(hash1));
        csha256.Finalize(dest);
    }

   private:
    static CSHA256 csha256;
};
#endif