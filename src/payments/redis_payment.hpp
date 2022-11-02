#ifndef REDIS_PAYMENT_HPP_
#define REDIS_PAYMENT_HPP_
#include "redis_manager.hpp"

class RedisPayment : public RedisManager {

    public:
     using RedisManager::RedisManager;

     bool SubscribeToMaturityChannel() {
         Command({"SUBSCRIBE", key_names.block_mature_channel});
         return true;
     }
};

#endif