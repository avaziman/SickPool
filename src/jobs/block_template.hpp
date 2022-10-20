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

struct TransactionData
{
   public:
    // TransactionData(std::string_view data_hex) : data_hex(data_hex)
    // {ini
    //     data.resize(data_hex.size() / 2);
    //     Unhexlify(data.data(), data_hex.data(), data_hex.size());

    //     HashWrapper::SHA256d(hash, data.data(), data.size());
    // }

    std::string_view data_hex;
    std::vector<uint8_t> data;
    uint8_t hash[HASH_SIZE];
    char hash_hex[HASH_SIZE_HEX];
    int64_t fee = 0;

    TransactionData(std::string_view data_hex, std::string_view hashhex)
        : data_hex(data_hex)
    {
        data.resize(data_hex.size() / 2);
        Unhexlify(data.data(), data_hex.data(), data_hex.size());

        Unhexlify(hash, hashhex.data(), hashhex.size());
        std::reverse(hash, hash + HASH_SIZE);

        memcpy(hash_hex, hashhex.data(), HASH_SIZE_HEX);
    }

    TransactionData(std::string_view data_hex) : data_hex(data_hex)
    {
        data.resize(data_hex.size() / 2);
        Unhexlify(data.data(), data_hex.data(), data_hex.size());

        Hash();
    }

    void Hash(){
        // no need to reverse here, as in block encoding
        HashWrapper::SHA256d(hash, data.data(), data.size());

        // the hex does need to be reversed
        Hexlify(hash_hex, hash, HASH_SIZE);
        ReverseHex(hash_hex, hash_hex, HASH_SIZE_HEX);
    }

    TransactionData() = default;
};

struct TransactionDataList
{
    std::vector<TransactionData> transactions;
    std::size_t byte_count = 0;

    TransactionDataList(const std::size_t max_tx_count)
    {
        transactions.reserve(max_tx_count);
    }

    bool AddTxData(const TransactionData& td)
    {
        std::size_t tx_size = td.data.size();

        // 2 bytes for tx count (max 65k)
        if (byte_count + tx_size + BLOCK_HEADER_SIZE + 2 > MAX_BLOCK_SIZE)
            return false;

        transactions.emplace_back(td);
        byte_count += tx_size;

        return true;
    }
};

struct BlockTemplate
{
    BlockTemplate() : tx_list(0) {}
    BlockTemplate(int32_t ver, std::string_view prev_bhash,
                  std::size_t max_tx_count, int64_t cb_val, int64_t min_time,
                  uint32_t bits, uint32_t height)
        : version(ver),
          prev_block_hash(prev_bhash),
          tx_list(max_tx_count),
          coinbase_value(cb_val),
          min_time(min_time),
          bits(bits),
          height(height),
          target_diff(BitsToDiff(bits)),
          expected_hashes(GetExpectedHashes(target_diff)),
          tx_count(max_tx_count - 1)
    {
    }

    int32_t version;
    std::string_view prev_block_hash;
    TransactionDataList tx_list;
    int64_t coinbase_value;
    int64_t min_time;
    uint32_t bits;
    uint32_t height;
    uint32_t block_size;
    uint32_t tx_count;

    double target_diff;
    double expected_hashes;
};

// in the order they appear in the rpc response
struct BlockTemplateVrsc : public BlockTemplate
{
    std::string_view finals_root_hash;
    std::string_view solution;
    std::string_view coinbase_hex;
    std::string_view target;
};

#endif