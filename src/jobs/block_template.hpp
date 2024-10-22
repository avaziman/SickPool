#ifndef BLOCK_TEMPLATE_HPP
#define BLOCK_TEMPLATE_HPP

#include <algorithm>
#include <ctime>
#include <span>
#include <string_view>
#include <vector>

#include "static_config/static_config.hpp"
#include "utils.hpp"
#include "hash_wrapper.hpp"

// struct TransactionData
// {
//    public:
//     std::string_view data_hex;
//     std::vector<uint8_t> data;
    
//     std::string_view hash_hex;
//     std::vector<uint8_t> hash;
//     int64_t fee = 0;

//     explicit TransactionData(std::string_view data_hex, std::string_view hashhex)
//         : data_hex(data_hex)
//     {
//         // data.resize(data_hex.size() / 2);
//         // Unhexlify(data.data(), data_hex.data(), data_hex.size());

//         // Unhexlify(hash.data(), hashhex.data(), hashhex.size());
//         // std::reverse(hash.data(), hash.data(), hashhex.size() / 2);
//     }

//     void Hash(){
//         // no need to reverse here, as in block encoding
//         // HashWrapper::SHA256d(hash.data(), data.data(), data.size());

//         // the hex does need to be reversed
//         // Hexlify(hash_hex.data(), hash.data(), hash_hex.size() / 2);
//         // ReverseHex(hash_hex.data(), hash_hex.data(), hash_hex.size());
//     }

//     TransactionData() = default;
// };

// struct TransactionDataList
// {
//     std::vector<TransactionData> transactions;
//     std::size_t byte_count = 0;

//     explicit TransactionDataList(const std::size_t max_tx_count)
//     {
//         transactions.reserve(max_tx_count);
//     }

//     bool AddTxData(const TransactionData& td)
//     {
//         std::size_t tx_size = td.data.size();

//         // 2 bytes for tx count (max 65k)
//         if (byte_count + tx_size + BLOCK_HEADER_SIZE + 2 > MAX_BLOCK_SIZE)
//             return false;

//         transactions.emplace_back(td);
//         byte_count += tx_size;

//         return true;
//     }
// };

// struct BlockTemplate
// {
//     BlockTemplate() = default;                              
//     BlockTemplate(int32_t ver, std::string_view prev_bhash,
//                   std::size_t max_tx_count, int64_t cb_val, int64_t min_time,
//                   uint32_t bits, uint32_t height)
//         : version(ver),
//           prev_block_hash(prev_bhash),
//           tx_list(max_tx_count),
//           coinbase_value(cb_val),
//           min_time(min_time),
//           bits(bits),
//           height(height),
//           // TODO: fix
//           tx_count(static_cast<uint32_t>(max_tx_count - 1)),
//           // target_diff(BitsToDiff(bits)),
//           target_diff((bits)),  // TODO: use target...
//           expected_hashes(/*GetHashMultiplier<confs>*/ target_diff)
//     {
//     }

//     int32_t version;
//     std::string_view prev_block_hash;
//     TransactionDataList tx_list;
//     int64_t coinbase_value;
//     int64_t min_time;
//     uint32_t bits;
//     uint32_t height;
//     uint32_t tx_count;
//     uint32_t block_size;

//     double target_diff;
//     double expected_hashes;
// };

// in the order they appear in the rpc response
// struct BlockTemplateVrsc : public BlockTemplate
// {
//     std::string_view finals_root_hash;
//     std::string_view solution;
//     std::string_view coinbase_hex;
//     std::string_view target;
// };

#endif