#ifndef REDIS_TRANSACTION_HPP
#define REDIS_TRANSACTION_HPP

#include <hiredis/hiredis.h>

class RedisTransaction
{
   public:
    RedisTransaction(redisContext* rc, int& command_count) : rc(rc)
    {
        redisAppendCommand(rc, "MULTI");
        command_count += 2;
    }
    ~RedisTransaction() { redisAppendCommand(rc, "EXEC"); }

   private:
    redisContext* rc;
};

#endif