#include "hash_wrapper.hpp"
#include <chrono>

CVerusHashV2 HashWrapper::cverusHashV2(SOLUTION_VERUSHHASH_V2_2);
CSHA256 HashWrapper::csha256;

void HashWrapper::InitVerusHash() {
    CVerusHashV2::init();
    // CVerusHash::init();
    // cverusHashV2 = CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);
}

void HashWrapper::InitSHA256()
{
    csha256 = CSHA256();
}

void HashWrapper::VerushashV2b2(unsigned char* dest, unsigned char* in,
                                int size, CVerusHashV2* hasher)

{
    
    hasher->Write(in, size);
    // auto start = std::chrono::steady_clock::now();
    hasher->Finalize2b(dest);
    // auto end = std::chrono::steady_clock::now();
    hasher->Reset();
    // CVerusHash::Hash(dest, in, size);
}
void HashWrapper::SHA256d(unsigned char* dest, unsigned char* in, int size)
{
    // Make sure its initialized before this

    unsigned char hash1[32];

    csha256.Reset();
    csha256.Write(in, size);
    csha256.Finalize(hash1);

    csha256.Reset();

    csha256.Write(hash1, 32);
    csha256.Finalize(dest);
}