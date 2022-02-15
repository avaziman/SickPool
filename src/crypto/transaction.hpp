#ifndef TRANSACTION_HPP_
#define TRANSACTION_HPP_

#include <bit>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "base58.h"
#include "utils.hpp"

#define PUBKEYHASH_BYTES 20

#define OP_DUP 0x76
#define OP_HASH160 0xa9
#define OP_EQUALVERIFY 0x88
#define OP_CHECKSIG 0xac
#define OP_CRYPTOCONDITION = 0xfc

    // the specific part of a specific output list in a transaction (txid +
    // index)
    struct OutPoint
{
    std::string hash;  // txid
    uint32_t index;
};

struct Output
{
    int64_t value;  // number of satoshis
    std::string pk_script;
};

struct Input
{
    OutPoint previous_output;
    std::string signature_script;
    uint32_t sequence;
    // supposed to be used for replacement, set to UINT32_MAX to mark as final
};

class Transaction
{
   protected:
    int32_t version;
    std::vector<Input> vin;
    std::vector<Output> vout;
    uint32_t lock_time;

    std::string GetScript(std::string toAddress)
    {
        std::vector<unsigned char> vchRet;
        bool res = DecodeBase58(toAddress, vchRet);

        std::stringstream script;
        script << std::hex << OP_DUP << OP_HASH160;

        script << std::hex << std::setfill('0') << std::setw(2)
               << PUBKEYHASH_BYTES;

        for (int i = 1; i <= PUBKEYHASH_BYTES; i++)
            script << std::hex << std::setfill('0') << std::setw(2)
                   << int(vchRet[i]);

        script << std::hex << OP_EQUALVERIFY << OP_CHECKSIG;
        return script.str();
    }

   public:
    Transaction(int32_t ver, uint32_t locktime) : version(ver) , lock_time(locktime){}
    void AddInput(std::string prevTxid, uint32_t prevIndex, std::string signature, uint32_t sequence) {
        Input input;
        OutPoint point;
        point.hash = prevTxid;
        point.index = prevIndex;
        input.previous_output = point;
        input.signature_script = signature;
        input.sequence = sequence;
        vin.push_back(input);
    }

    // standard p2pkh transaction
    void AddStdOutput(std::string toAddress, int64_t value)
    {
        Output output;
        output.value = value;
        output.pk_script = GetScript(toAddress);
        vout.push_back(output);
    }

    // std::string GetCoinbase1() { return coinbase1; }
    // std::string GetCoinbase2() { return coinbase2; }

    virtual std::string GetHex() = 0;
};
#endif