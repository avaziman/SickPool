#ifndef TRANSACTION_HPP_
#define TRANSACTION_HPP_

#include <bit>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "base58.h"
#include "utils.hpp"

#define PUBKEYHASH_BYTES_LEN 20
#define STD_SCRIPT_SIZE (PUBKEYHASH_BYTES_LEN + 5)

#define OP_DUP 0x76
#define OP_HASH160 0xa9
#define OP_EQUALVERIFY 0x88
#define OP_CHECKSIG 0xac
#define OP_CRYPTOCONDITION 0xfc

// the specific part of a specific output list in a transaction (txid +
// index)
struct OutPoint
{
    unsigned char hash[32];  // txid
    uint32_t index;
};

struct Output
{
    int64_t value;  // number of satoshis
    std::vector<unsigned char> pk_script;
};

struct Input
{
    OutPoint previous_output;
    std::vector<unsigned char> signature_script;
    uint32_t sequence;
    // supposed to be used for replacement, set to UINT32_MAX to mark as final
};

class Transaction
{
   protected:
    std::vector<unsigned char> bytes;
    int32_t version;
    std::vector<Input> vin;
    std::vector<Output> vout;
    uint32_t lock_time;

    void GetScript(const char* toAddress, std::vector<unsigned char> &res)
    {
        const unsigned char pubkeyhash_bytes_len = PUBKEYHASH_BYTES_LEN;

        std::vector<unsigned char> vchRet;
        bool decRes = DecodeBase58(toAddress, vchRet);

        int written = 0;

        memcpy(res + written, &OP_DUP, 1);
        written += 1;
        memcpy(res + written, &OP_HASH160, 1);
        written += 1;
        memcpy(res + written, &pubkeyhash_bytes_len, 1);
        written += 1;
        memcpy(res + written, vchRet.data(), pubkeyhash_bytes_len);
        written += pubkeyhash_bytes_len;
        memcpy(res + written, &OP_EQUALVERIFY, 1);
        written += 1;
        memcpy(res + written, &OP_CHECKSIG, 1);
        written += 1;
    }

   public:
    Transaction(int32_t ver, uint32_t locktime)
        : version(ver), lock_time(locktime)
    {
    }
    void AddInput(unsigned char* prevTxId, uint32_t prevIndex,
                  std::vector<unsigned char> signature, uint32_t sequence)
    {
        Input input;
        OutPoint point;
        memcpy(point.hash, prevTxId, 32);
        point.index = prevIndex;
        input.previous_output = point;
        input.signature_script = signature;
        input.sequence = sequence;
        vin.push_back(input);
    }

    // standard p2pkh transaction
    void AddStdOutput(const char* toAddress, int64_t value)
    {
        Output output;
        output.value = value;
        GetScript(toAddress, output.pk_script.data());
        vout.push_back(output);
    }

    // std::string GetCoinbase1() { return coinbase1; }
    // std::string GetCoinbase2() { return coinbase2; }

    virtual std::vector<unsigned char>* GetBytes() = 0;
};
#endif