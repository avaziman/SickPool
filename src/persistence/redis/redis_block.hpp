#ifndef REDIS_BLOCK_HPP_
#define REDIS_BLOCK_HPP_
#include "block_submission.hpp"
#include "mysql_manager.hpp"
#include "persistence_layer.hpp"
#include "redis_manager.hpp"
#include <optional>

class PersistenceBlock : public PersistenceLayer
{
    protected:
     const BlockKeyNames block_key_names;

    public:
     explicit PersistenceBlock(const PersistenceLayer& pl);

     void AppendUpdateBlockHeight(sw::redis::Pipeline& pipe, uint32_t number);
     uint32_t GetBlockHeight();
};

#endif