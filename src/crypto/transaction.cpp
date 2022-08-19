#include "transaction.hpp"

void Transaction::AddInput(const uint8_t* prevTxId, uint32_t prevIndex,
              std::vector<uint8_t> signature, uint32_t sequence)
{
    Input input;
    OutPoint point;
    memcpy(point.txid_hash, prevTxId, 32);
    point.index = prevIndex;

    input.previous_output = point;
    input.signature_script = signature;
    input.sequence = sequence;

    uint64_t varIntVal = signature.size();
    char varIntLen = VarInt(varIntVal);

    input.sig_compact_val = varIntVal;
    input.sig_compact_len = varIntLen;

    vin.push_back(input);
    tx_len += varIntLen + input.signature_script.size() + sizeof(OutPoint) +
              sizeof(sequence);
}

void Transaction::AddP2PKHOutput(std::string_view toAddress, int64_t value)
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

void Transaction::AddOutput(const std::vector<uint8_t>& script_pub_key, int64_t value)
{
    Output output;
    output.value = value;

    output.pk_script = script_pub_key;

    uint64_t varIntVal = output.pk_script.size();
    char varIntLen = VarInt(varIntVal);

    output.script_compact_val = varIntVal;
    output.script_compact_len = varIntLen;

    vout.push_back(output);
    tx_len += varIntLen + output.pk_script.size() + sizeof(output.value);
}