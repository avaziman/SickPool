// #include "static_config.hpp"

// #if SICK_COIN == SIN || SICK_COIN == ZANO
// #include "transaction_btc.hpp"
// std::size_t TransactionBtc::GetBytes(std::vector<uint8_t>& bytes)
// {
//     std::vector<uint8_t> vin_num_script = GenNumScript(vin.size());
//     std::vector<uint8_t> vout_num_script = GenNumScript(vout.size());

//     tx_len += 2 * 4 + vin_num_script.size() + vout_num_script.size();
//     bytes.resize(tx_len);

//     WriteData(bytes.data(), &TXVERSION, VERSION_SIZE);

//     WriteData(bytes.data(), vin_num_script.data(), vin_num_script.size());

//     for (Input input : this->vin)
//     {
//         WriteData(bytes.data(), input.previous_output.txid_hash, HASH_SIZE);
//         WriteData(bytes.data(), &input.previous_output.index,
//                   sizeof(Input::previous_output.index));
//         // WriteData(bytes.data(), &input.sig_compact_val, input.sig_compact_len);
//         WriteData(bytes.data(), input.signature_script.data(),
//                   input.signature_script.size());

//         WriteData(bytes.data(), &input.sequence, sizeof(Input::sequence));
//     }

//     WriteData(bytes.data(), vout_num_script.data(), vout_num_script.size());
//     for (Output output : this->vout)
//     {
//         WriteData(bytes.data(), &output.value, sizeof(Output::value));

//         // WriteData(bytes.data(), &output.script_compact_val,
//         //           output.script_compact_len);
//         WriteData(bytes.data(), output.pk_script.data(),
//                   output.pk_script.size());
//     }

//     WriteData(bytes.data(), &lock_time, sizeof(lock_time));
//     return VERSION_SIZE + vin_num_script.size();
// }

// #endif