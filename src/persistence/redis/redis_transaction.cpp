#include "redis_transaction.hpp"

RedisTransaction::RedisTransaction(RedisManager* redis_manager) : redis_manager(redis_manager)
{
    redis_manager->AppendCommand({std::string_view("MULTI")});
}

RedisTransaction::~RedisTransaction() {
    redis_manager->AppendCommand({std::string_view("EXEC")});
}
