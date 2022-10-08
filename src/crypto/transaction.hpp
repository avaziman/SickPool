#ifndef TRANSACTION_HPP_
#define TRANSACTION_HPP_

#include <bit>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "base58.h"
#include "utils.hpp"

constexpr auto PUBKEYHASH_BYTES_LEN = 20;
constexpr auto P2PKH_SCRIPT_SIZE = PUBKEYHASH_BYTES_LEN + 5;

constexpr auto OP_DUP = 0x76;
constexpr auto OP_HASH160 = 0xa9;
constexpr auto OP_EQUALVERIFY = 0x88;
constexpr auto OP_CHECKSIG = 0xac;
constexpr auto OP_CRYPTOCONDITION = 0xfc;

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
    std::vector<uint8_t> pk_script;

    template <typename T>
    Output(int64_t val, T script) : value(val)
    {
        SetScript(script);
    }

    Output(){
        // empty (reserved)
    }

    void SetScript(std::string_view hex)
    {
        auto byte_count = hex.size() / 2;
        auto num_script = GenNumScript(byte_count);
        pk_script.resize(byte_count + num_script.size());

        memcpy(pk_script.data(), num_script.data(), num_script.size());
        Unhexlify(pk_script.data() + num_script.size(), hex.data(), hex.size());
    }

    void SetScript(const std::vector<uint8_t>& script)
    {
        auto byte_count = script.size();
        auto num_script = GenNumScript(byte_count);
        pk_script.resize(byte_count + num_script.size());

        memcpy(pk_script.data(), num_script.data(), num_script.size());
        memcpy(pk_script.data() + num_script.size(), script.data(),
               script.size());
    }
};

struct Input
{
    OutPoint previous_output;
    std::vector<uint8_t> signature_script;
    // supposed to be used for replacement, set to UINT32_MAX to mark as final
    uint32_t sequence;

    template <typename T>
    Input(OutPoint prev_outpoint, T sig, uint32_t seq = UINT32_MAX)
        : previous_output(prev_outpoint), sequence(seq)
    {
        SetSignatureScript(sig);
    }

    void SetSignatureScript(std::string_view hex)
    {
        auto byte_count = hex.size() / 2;
        auto num_script = GenNumScript(byte_count);
        signature_script.resize(byte_count + num_script.size());

        memcpy(signature_script.data(), num_script.data(), num_script.size());
        Unhexlify(signature_script.data() + num_script.size(), hex.data(),
                  hex.size());
    }

    void SetSignatureScript(const std::vector<uint8_t>& sig)
    {
        auto byte_count = sig.size();
        auto num_script = GenNumScript(byte_count);
        signature_script.resize(byte_count + num_script.size());

        memcpy(signature_script.data(), num_script.data(), num_script.size());
        memcpy(signature_script.data() + num_script.size(), sig.data(),
               sig.size());
    }
};

class Transaction
{
   protected:
    int tx_len = 0;
    // std::vector<unsigned char> bytes;
    // std::vector<Input> vin;
    // std::vector<Output> vout;
    uint32_t lock_time;

    template <typename T>
    inline void WriteData(unsigned char* bytes, T* val, int size)
    {
        memcpy(bytes + written, val, size);
        written += size;
    }

    int written = 0;

   public:
    Transaction(std::size_t max_vin_size = 0, std::size_t max_vout_size = 0,
                uint32_t locktime = 0x00000000)
        : lock_time(locktime)
    {
        vin.reserve(max_vin_size);
        vout.reserve(max_vout_size);
    }

    static void GetP2PKHScript(std::string_view toAddress,
                               std::vector<unsigned char>& res)
    {
        std::vector<unsigned char> vchRet;
        // TODO: implement base58 decode for string_view
        bool decRes = DecodeBase58(std::string(toAddress), vchRet);

        res.reserve(P2PKH_SCRIPT_SIZE);

        res.push_back(OP_DUP);
        res.push_back(OP_HASH160);
        res.push_back(PUBKEYHASH_BYTES_LEN);
        res.insert(res.end(), ++vchRet.begin(), ++vchRet.begin() + PUBKEYHASH_BYTES_LEN);
        res.push_back(OP_EQUALVERIFY);
        res.push_back(OP_CHECKSIG);
    }

    // std::vector<Output>* GetOutputs() { return &vout; }
    int GetTxLen() const { return tx_len; }

    void AddInput(const uint8_t* prevTxId, uint32_t prevIndex,
                  const std::vector<uint8_t>& signature, uint32_t sequence);

    // standard p2pkh transaction
    void AddP2PKHOutput(std::string_view toAddress, int64_t value);

    void SwitchOutput(int index, const Output& output);
    void AddOutput(const std::vector<uint8_t>& script_pub_key, int64_t value);
    void AddOutput(const Output& output);
    // virtual void GetBytes(std::vector<uint8_t>& bytes);
    // TODO: make public func
    void SetOutputs(std::vector<Output>& outputs) { vout = std::move(outputs); }

    std::vector<Output> vout;
    std::vector<Input> vin;
    };

#if SICK_COIN == VRSC
#include "transaction_vrsc.hpp"
#elif SICK_COIN == SIN
#include "transaction_btc.hpp"
#elif SICK_COIN == ZANO
#include "transaction_btc.hpp"
#endif
#endif