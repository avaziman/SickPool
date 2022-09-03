#ifndef REDIS_TRANSACTION_HPP
#define REDIS_TRANSACTION_HPP

#include <hiredis/hiredis.h>
#include "redis_manager.hpp"

class RedisManager;
class RedisTransaction
{
   public:
    RedisTransaction(RedisManager* redis_manager);
    ~RedisTransaction();

   private:
    RedisManager* redis_manager;
};

#endif