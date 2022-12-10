#ifndef REDIS_PAYMENT_HPP_
#define REDIS_PAYMENT_HPP_
#include "redis_manager.hpp"
#include "redis_block.hpp"

class RedisPayment : public RedisBlock {
    using RedisBlock::RedisBlock;
};

#endif