#ifndef HASH_WRAPPER_HPP
#define HASH_WRAPPER_HPP

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "verushash/sha256.h"
#include "verushash/verus_hash.h"
#include "verushash/verus_clhash.h"

#define VERUS_HASH_SIZE 32

class HashWrapper
{
   public:
    static void InitVerusHash();
    static void InitSHA256();
    static void VerushashV2b2(unsigned char* dest, unsigned char* in, int size);
    static void SHA256d(unsigned char* dest, unsigned char* in, int size);

   private:
    static CVerusHashV2 cverusHashV2;
    static CSHA256 csha256;
};
#endif