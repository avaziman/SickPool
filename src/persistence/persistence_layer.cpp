#include "persistence_layer.hpp"

PersistenceLayer::PersistenceLayer(const CoinConfig& cc) : RedisManager(cc), MySqlManager(cc){

}