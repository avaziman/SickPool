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
    unsigned char txid_hash[32];  // txid
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
    // supposed to be used for replacement, set to UINT32_MAX to mark as final
    uint32_t sequence;

    uint64_t sig_compact_val;
    char sig_compact_len;
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

    void GetP2PKHScript(std::string_view toAddress,
                        std::vector<unsigned char>& res) const
    {
        std::vector<unsigned char> vchRet;
        // TODO: implement base58 decode for string_view
        bool decRes = DecodeBase58(std::string(toAddress), vchRet);

        res.reserve(P2PKH_SCRIPT_SIZE);

        res.push_back(OP_DUP);
        res.push_back(OP_HASH160);
        res.push_back(PUBKEYHASH_BYTES_LEN);
        res.insert(res.end(), vchRet.begin(), vchRet.begin() + 20);
        res.push_back(OP_EQUALVERIFY);
        res.push_back(OP_CHECKSIG);
    }

   public:
    Transaction(int32_t ver, uint32_t locktime)
        : version(ver), lock_time(locktime)
    {
    }

    std::vector<Output>* GetOutputs() { return &vout; }
    int GetTxLen() const { return tx_len; }

    void AddInput(const uint8_t* prevTxId, uint32_t prevIndex,
                  std::vector<uint8_t> signature, uint32_t sequence);

    // standard p2pkh transaction
    void AddP2PKHOutput(std::string_view toAddress, int64_t value);

    void AddOutput(const std::vector<uint8_t>& script_pub_key, int64_t value);

    // std::string GetCoinbase1() { return coinbase1; }
    // std::string GetCoinbase2() { return coinbase2; }

    virtual void GetBytes(std::vector<uint8_t>& bytes) = 0;
};
#endif