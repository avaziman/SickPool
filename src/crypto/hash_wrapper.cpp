#include "hash_wrapper.hpp"
#include <chrono>
bool HashWrapper::VERUSHASH_V2b2_INITIALIZED = false;
CVerusHashV2* HashWrapper::cverusHashV2 = nullptr;

void HashWrapper::InitVerusHash() { CVerusHashV2::init();
    CVerusHashV2::init();
    cverusHashV2 = new CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);
}

void HashWrapper::VerushashV2b2(char* in, int size, char* res){
    // Make sure its initialized before this
    // cverusHashV2->Reset();
    // cverusHashV2->Write((unsigned char*)in, size);
    // cverusHashV2->Finalize2b((unsigned char*)res);
    // CVerusHashV2::Hash(res, in, size);

    

}
void HashWrapper::SHA256d(char* in, int size, char* res)
{
    char hash1[32];

    CSHA256 sha256;
    sha256.Write((unsigned char*)in, size);
    sha256.Finalize((unsigned char*)hash1);

    sha256.Reset();

    sha256.Write((unsigned char*)hash1, 32);
    sha256.Finalize((unsigned char*)res);
}