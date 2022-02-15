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
    static void VerushashV2b2(char* in, int size, char* res);
    static void SHA256d(char* in, int size, char* res);

   private:
    static bool VERUSHASH_V2b2_INITIALIZED;
    static CVerusHashV2* cverusHashV2;
};
#endif