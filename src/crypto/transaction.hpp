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
#define P2PKH_SCRIPT_SIZE (PUBKEYHASH_BYTES_LEN + 5)

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

    uint64_t script_compact_val;
    char script_compact_len;
};

struct Input
{
    OutPoint previous_output;
    std::vector<unsigned char> signature_script;
    uint32_t sequence;

    uint64_t sig_compact_val;
    char sig_compact_len;
    // supposed to be used for replacement, set to UINT32_MAX to mark as final
};

class Transaction
{
   protected:
    int tx_len = 0;
    // std::vector<unsigned char> bytes;
    int32_t version;
    std::vector<Input> vin;
    std::vector<Output> vout;
    uint32_t lock_time;

    void GetP2PKHScript(const char* toAddress, std::vector<unsigned char> &res)
    {
        std::vector<unsigned char> vchRet;
        bool decRes = DecodeBase58(toAddress, vchRet);

        res.resize(P2PKH_SCRIPT_SIZE);
        res[0] = OP_DUP;
        res[1] = OP_HASH160;
        res[2] = PUBKEYHASH_BYTES_LEN;
        for (int i = 0; i < PUBKEYHASH_BYTES_LEN; i++) res[3 + i] = vchRet[i + 1];
        res[2 + PUBKEYHASH_BYTES_LEN + 1] = OP_EQUALVERIFY;
        res[2 + PUBKEYHASH_BYTES_LEN + 2] = OP_CHECKSIG;

        // res.push_back(OP_DUP);
        // res.push_back(OP_HASH160);
        // res.push_back(PUBKEYHASH_BYTES_LEN);
        // res.insert(res.end(), vchRet.begin(), vchRet.begin() + 20);
        // res.push_back(OP_EQUALVERIFY);
        // res.push_back(OP_CHECKSIG);
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

        uint64_t varIntVal = signature.size();
        char varIntLen = VarInt(varIntVal);

        input.sig_compact_val = varIntVal;
        input.sig_compact_len = varIntLen;

        vin.push_back(input);
        tx_len += varIntLen + input.signature_script.size() + sizeof(OutPoint) + sizeof(sequence);
    }

    // standard p2pkh transaction
    void AddStdOutput(const char* toAddress, int64_t value)
    {
        Output output;
        output.value = value;
        GetP2PKHScript(toAddress, output.pk_script);

        uint64_t varIntVal = output.pk_script.size();
        char varIntLen = VarInt(varIntVal);

        output.script_compact_val = varIntVal;
        output.script_compact_len = varIntLen;

        vout.push_back(output);
        tx_len += varIntLen + output.pk_script.size() + sizeof(output.value);
    }

    // std::string GetCoinbase1() { return coinbase1; }
    // std::string GetCoinbase2() { return coinbase2; }

    virtual std::vector<unsigned char> GetBytes() = 0;
};
#endif