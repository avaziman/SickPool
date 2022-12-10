#ifndef PERSISTENCE_STATS_HPP_
#define PERSISTENCE_STATS_HPP_
#include "persistence_layer.hpp"
#include "redis_stats.hpp"
#include "mysql_manager.hpp"

class PersistenceStats : public RedisStats, public MySqlManager
{
   public:
    explicit PersistenceStats(const PersistenceLayer& pl) : RedisStats(pl), MySqlManager(pl){};
};

#endif