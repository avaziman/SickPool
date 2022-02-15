#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <sstream>
#include <vector>

#include "block_header.hpp"
#include "merkle_tree.hpp"
#include "utils.hpp"

#define VERUS_HEADER_SIZE (4 + 32 + 32 + 32 + 4 + 4 + 32 + 3 + 1344) * 2

class Block
{
   public:
    Block(BlockHeader* bh, void (*txHash)(char*, int, char*),
          void (*blockHash)(char*, int, char*))
        : block_header(bh), TxHash(txHash), BlockHash(blockHash)
    {
    }
    void AddTransaction(std::string txHex)
    {
        transactions.push_back(txHex);

        merkle_tree.AddTx((char*)txHex.c_str(), txHex.size());

        // std::cout << "tx: " << txHex << std::endl;
    }

    char* GetMerkleRoot() { return merkle_root; }

    char* GetHex(char* time, char* merkleRootHash, char* nonce)
    {
        // std::stringstream ss;

        // ss << block_header->GetHex(time, merkleRootHash, nonce);
        // ss << VarInt(transactions.size());
        // for (std::string tx : transactions) ss << tx;
        // return ss.str();
        return nullptr;
    }

    std::string GetHex(std::string headerHex)
    {
        std::stringstream ss;

        ss << headerHex;
        ss << VarInt(transactions.size());
        for (std::string tx : transactions) ss << tx;
        return ss.str();
    }

    void GetHash(char* headerHex, char* res)
    {
        BlockHash(headerHex, VERUS_HEADER_SIZE, res);
    }

    BlockHeader* Header() { return block_header; }

    std::vector<std::string> GetTransactions() { return transactions; }

    void CalcMerkleRoot()
    {
        // we only need to calculate the merkle root once per job

        this->merkle_tree.CalcRoot(merkle_root);
        merkle_root[32] = 0;  // NULL terminated for sprintf
    }

   private:
    BlockHeader* block_header;
    std::vector<std::string> transactions;
    MerkleTree merkle_tree;
    void (*BlockHash)(char*, int, char*);
    void (*TxHash)(char*, int, char*);
    char merkle_root[33];
};

#endif