#ifndef PERSISTENCE_LAYER_HPP_
#define PERSISTENCE_LAYER_HPP_
#include "mysql_manager.hpp"
#include "redis_manager.hpp"

class PersistanceLayer 
{
    RedisManager rm;
    MySqlManager mm;
};

#endif