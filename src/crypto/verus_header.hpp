// #ifndef VERUS_BLOCK_HEADER_HPP
// #define VERUS_BLOCK_HEADER_HPP

// #include <chrono>
// #include <iostream>

// #include "block_header.hpp"
// #include "utils.hpp"
// #include "verushash/endian.h"

// class VerusBlockHeader : public Job
// {
//    public:
//     VerusBlockHeader(char* ver, char* prevBlock, char* time, char* bits,
//                      char* finalsRoot, char* sol)
//         : Job(ver, prevBlock, time, bits)
//     {
//         // don't use dynamic memory as it's slower

//         memcpy(this->finalSaplingRoot, finalsRoot, 64 + 1);
//         memcpy(this->solution144, sol, 144);

//         memcpy(this->hex, ver, 8);
//         memcpy(this->hex + 8, prevBlock, 64);
//         // skip merkle root hash because we set it later
//         memcpy(this->hex + 8 + 64 + 64, finalsRoot, 8);
//         // skip time because we set it later
//         memcpy(this->hex + 8 + 64 + 64 + 8, bits, 8);
//     }

//     inline char* GetHex(char* time, char* merkleRootHash,
//                         char* nonce) override
//     {
//         auto now = std::chrono::steady_clock::now();

//         memcpy(this->hex + 8 + 64, merkleRootHash, 64);
//         memcpy(this->hex + 8 + 64 + 64, time, 8);
//         memcpy(this->hex + 8 + 64 + 64 + 8 + 8, nonce, 64 + 2688);

//         auto end = std::chrono::steady_clock::now();
//         std::cout << "block serialization took: "
//                   << std::chrono::duration_cast<std::chrono::microseconds>(end -
//                                                                            now)
//                          .count()
//                   << "microseconds" << std::endl;
//         return this->hex;
//         // return res;
//     }

//     // inline char* GetFinalSaplingRoot() { return this->finalSaplingRoot; }

//     // inline char* GetSol144() { return this->solution144; }

//    private:
//     char hex[VERUS_HEADER_SIZE];
//     char finalSaplingRoot[64];
//     char solution144[144];
    
// };

// #endif