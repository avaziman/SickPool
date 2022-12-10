#include <array>
#include <cstdint>
#include <type_traits>

enum class Prefix : ::std::uint8_t;
enum class BlockType : ::std::uint8_t;
struct BlockSubmission;

#ifndef CXXBRIDGE1_ENUM_Prefix
#define CXXBRIDGE1_ENUM_Prefix
enum class Prefix : ::std::uint8_t {
  POW = 0,
  PAYOUT = 1,
  PAYOUT_FEELESS = 2,
  PAYOUTS = 3,
  ADDRESS = 4,
  ADDRESS_ID_MAP = 5,
  ALIAS = 6,
  PAYOUT_THRESHOLD = 7,
  IDENTITY = 8,
  ROUND = 9,
  EFFORT = 10,
  WORKER_COUNT = 11,
  MINER_COUNT = 12,
  TOTAL_EFFORT = 13,
  ESTIMATED_EFFORT = 14,
  START_TIME = 15,
  MATURE_BALANCE = 16,
  IMMATURE_BALANCE = 17,
  MATURE = 18,
  IMMATURE = 19,
  REWARD = 20,
  HASHRATE = 21,
  SHARES = 22,
  BLOCK = 23,
  MINED_BLOCK = 24,
  COUNT = 25,
  NETWORK = 26,
  POOL = 27,
  AVERAGE = 28,
  VALID = 29,
  INVALID = 30,
  STALE = 31,
  EFFORT_PERCENT = 32,
  SOLVER = 33,
  INDEX = 34,
  DURATION = 35,
  DIFFICULTY = 36,
  ROUND_EFFORT = 37,
  PAYEES = 38,
  FEE_PAYEES = 39,
  PENDING_AMOUNT = 40,
  PENDING_AMOUNT_FEE = 41,
  PENDING = 42,
  FEELESS = 43,
  MINER = 44,
  WORKER = 45,
  TYPE = 46,
  NUMBER = 47,
  CHAIN = 48,
  ACTIVE_IDS = 49,
  STATS = 50,
  COMPACT = 51,
};
#endif // CXXBRIDGE1_ENUM_Prefix

#ifndef CXXBRIDGE1_ENUM_BlockType
#define CXXBRIDGE1_ENUM_BlockType
enum class BlockType : ::std::uint8_t {
  POW = 1,
  PAYMENT = 1,
};
#endif // CXXBRIDGE1_ENUM_BlockType

#ifndef CXXBRIDGE1_STRUCT_BlockSubmission
#define CXXBRIDGE1_STRUCT_BlockSubmission
struct BlockSubmission final {
  ::std::int32_t confirmations;
  ::std::uint8_t block_type;
  ::std::uint8_t chain;
  ::std::uint64_t reward;
  ::std::uint64_t time_ms;
  ::std::uint64_t duration_ms;
  ::std::uint32_t height;
  ::std::uint32_t number;
  double difficulty;
  double effort_percent;
  ::std::uint32_t miner_id;
  ::std::uint32_t worker_id;
  ::std::array<::std::uint8_t, 32> hash_bin;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_BlockSubmission
