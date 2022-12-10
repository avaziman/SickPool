#include "persistence_layer.hpp"

PersistenceLayer::PersistenceLayer(const CoinConfig& cc) : RedisManager(cc), MySqlManager(cc){

}

PersistenceLayer::PersistenceLayer(const PersistenceLayer& pl): RedisManager(pl), MySqlManager(pl) {

}
