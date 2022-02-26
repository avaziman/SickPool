#include "hash_wrapper.hpp"
#include <chrono>

void HashWrapper::InitVerusHash() { CVerusHashV2::init();
    CVerusHashV2::init();
    // cverusHashV2 = new CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);
}

void HashWrapper::VerushashV2b2(unsigned char* in, int size, unsigned char* res){
    // Make sure its initialized before this
    CVerusHashV2 *cverusHashV2 = new CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);

    cverusHashV2->Reset();
    cverusHashV2->Write((unsigned char*)in, size);
    cverusHashV2->Finalize2b((unsigned char*)res);
    // CVerusHashV2::Hash(res, in, size);

}
void HashWrapper::SHA256d(unsigned char* in, int size, unsigned char* res)
{
    unsigned char hash1[32];

    CSHA256 sha256;
    sha256.Write(in, size);
    sha256.Finalize(hash1);

    sha256.Reset();

    sha256.Write(hash1, 32);
    sha256.Finalize(res);
}