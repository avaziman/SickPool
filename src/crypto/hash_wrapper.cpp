#include "hash_wrapper.hpp"
#include <chrono>

CSHA256 HashWrapper::csha256;

void HashWrapper::InitVerusHash() {
    CVerusHashV2::init();
}

void HashWrapper::InitSHA256()
{
    csha256 = CSHA256();
}

void HashWrapper::VerushashV2b2(unsigned char* dest, unsigned char* in,
                                int size, CVerusHashV2* hasher)

{
    // auto start = std::chrono::steady_clock::now();

    hasher->Write(in, size);
    hasher->Finalize2b(dest);
    hasher->Reset();

    // auto end = std::chrono::steady_clock::now();
    // std::cout << "VerusHashV2b2: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " microseconds" << std::endl;
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