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
  NETWORK = 25,
  POOL = 26,
  AVERAGE = 27,
  VALID = 28,
  INVALID = 29,
  STALE = 30,
  EFFORT_PERCENT = 31,
  SOLVER = 32,
  INDEX = 33,
  DURATION = 34,
  DIFFICULTY = 35,
  ROUND_EFFORT = 36,
  PAYEES = 37,
  FEE_PAYEES = 38,
  PENDING_AMOUNT = 39,
  PENDING_AMOUNT_FEE = 40,
  PENDING = 41,
  FEELESS = 42,
  MINER = 43,
  WORKER = 44,
  TYPE = 45,
  NUMBER = 46,
  CHAIN = 47,
  ACTIVE_IDS = 48,
  STATS = 49,
  COMPACT = 50,
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
