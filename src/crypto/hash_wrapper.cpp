#include "hash_wrapper.hpp"

CSHA256 HashWrapper::csha256;

void HashWrapper::InitVerusHash() {
    CVerusHashV2::init();
}

void HashWrapper::InitSHA256()
{
    csha256 = CSHA256();
}