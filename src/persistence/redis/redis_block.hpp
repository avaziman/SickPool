#ifndef REDIS_BLOCK_HPP_
#define REDIS_BLOCK_HPP_
#include "redis_manager.hpp"
#include "mysql_manager.hpp"
#include "persistence_layer.hpp"
class PersistenceBlock : public PersistenceLayer
{
    private:
     BlockKeyNames block_key_names;

    public:
     explicit PersistenceBlock(const PersistenceLayer& pl) : PersistenceLayer(pl), block_key_names(this->key_names.coin) {}

     bool UpdateBlockConfirmations(std::string_view block_id,
                                   int32_t confirmations);
     bool UpdateImmatureRewards(uint8_t chain, uint32_t block_num,
                                int64_t matured_time, bool matured);
     bool UpdateBlockNumber(int64_t time, uint32_t number);

     bool LoadImmatureBlocks(
         std::vector<std::unique_ptr<BlockSubmission>>& submsissions);

     bool SubscribeToMaturityChannel();
     
     uint32_t GetBlockNumber();
};

#endif