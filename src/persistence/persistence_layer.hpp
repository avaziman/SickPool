#ifndef PERSISTENCE_LAYER_HPP_
#define PERSISTENCE_LAYER_HPP_
#include "mysql_manager.hpp"
#include "redis_manager.hpp"

class PersistenceLayer : public RedisManager, public MySqlManager
{
   public:
    explicit PersistenceLayer(const CoinConfig& cc);
    explicit PersistenceLayer(const PersistenceLayer& pl);
};

#endif