#ifndef HASH_WRAPPER_HPP
#define HASH_WRAPPER_HPP

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "verushash/sha256.h"
#include "verushash/verus_hash.h"
#include "utils.hpp"

#define VERUS_HASH_SIZE 32

class HashWrapper
{
   public:
    static void InitVerusHash();
    static void VerushashV2b2(unsigned char* in, int size, unsigned char* res);
    static void SHA256d(unsigned char* in, int size, unsigned char* res);

   private:
    static bool VERUSHASH_V2b2_INITIALIZED;
    static CVerusHashV2* cverusHashV2;
};
#endif