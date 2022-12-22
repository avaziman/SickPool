#include <cstdint>

enum class Prefix : ::std::uint8_t;
enum class BlockStatus : ::std::uint8_t;

#ifndef CXXBRIDGE1_ENUM_Prefix
#define CXXBRIDGE1_ENUM_Prefix
enum class Prefix : ::std::uint8_t {
  POW = 0,
  ADDRESS = 1,
  ADDRESS_ID_MAP = 2,
  ALIAS = 3,
  IDENTITY = 4,
  ROUND = 5,
  EFFORT = 6,
  WORKER_COUNT = 7,
  MINER_COUNT = 8,
  TOTAL_EFFORT = 9,
  ESTIMATED_EFFORT = 10,
  START_TIME = 11,
  MATURE_BALANCE = 12,
  IMMATURE_BALANCE = 13,
  MATURE = 14,
  IMMATURE = 15,
  REWARD = 16,
  HASHRATE = 17,
  SHARES = 18,
  BLOCK = 19,
  MINED_BLOCK = 20,
  COUNT = 21,
  NETWORK = 22,
  POOL = 23,
  AVERAGE = 24,
  VALID = 25,
  INVALID = 26,
  STALE = 27,
  EFFORT_PERCENT = 28,
  SOLVER = 29,
  INDEX = 30,
  DURATION = 31,
  DIFFICULTY = 32,
  ROUND_EFFORT = 33,
  PENDING = 34,
  FEELESS = 35,
  MINER = 36,
  WORKER = 37,
  TYPE = 38,
  NUMBER = 39,
  CHAIN = 40,
  ACTIVE_IDS = 41,
  STATS = 42,
  COMPACT = 43,
  HEIGHT = 44,
};
#endif // CXXBRIDGE1_ENUM_Prefix

#ifndef CXXBRIDGE1_ENUM_BlockStatus
#define CXXBRIDGE1_ENUM_BlockStatus
enum class BlockStatus : ::std::uint8_t {
  PENDING = 1,
  CONFIRMED = 2,
  ORPHANED = 4,
  PAID = 8,
  PENDING_ORPHANED = 5,
};
#endif // CXXBRIDGE1_ENUM_BlockStatus
