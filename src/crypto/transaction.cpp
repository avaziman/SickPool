#include "transaction.hpp"

void Transaction::AddInput(const uint8_t* prevTxId, uint32_t prevIndex,
                           const std::vector<uint8_t>& signature,
                           uint32_t sequence)
{
    OutPoint point;
    memcpy(point.txid_hash, prevTxId, HASH_SIZE);
    point.index = prevIndex;

    Input input(point, signature);

    // uint64_t varIntVal = signature.size();
    // char varIntLen = VarInt(varIntVal);

    vin.push_back(input);
    // includes varint
    tx_len += /*varIntLen +*/ input.signature_script.size() + sizeof(OutPoint) +
              sizeof(sequence);
}

void Transaction::AddP2PKHOutput(std::string_view toAddress, int64_t value)
{
    std::vector<uint8_t> script;
    GetP2PKHScript(toAddress, script);

    Output output(value, script);

    AddOutput(output);
}

void Transaction::AddOutput(const std::vector<uint8_t>& script_pub_key,
                            int64_t value)
{
    Output output(value, script_pub_key);

    vout.push_back(output);
    // includes num script size
    tx_len += output.pk_script.size() + sizeof(output.value);
}

void Transaction::SwitchOutput(int index, const Output& output){
    tx_len -= vout[index].pk_script.size();
    tx_len += output.pk_script.size();
    vout[index] = output;
}

void Transaction::AddOutput(const Output& output)
{
    vout.push_back(output);
    // includes num script size
    tx_len += output.pk_script.size() + sizeof(output.value);
}