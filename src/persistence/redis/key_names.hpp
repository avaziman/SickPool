#ifndef KEY_NAMES_HPP_
#define KEY_NAMES_HPP_
#include <initializer_list>
#include <string>
#include <string_view>

#include "redis_interop.hpp"
#include "utils.hpp"
// TODO: make private...

struct Stringable
{
    const std::string_view val;

    // explicit(false) Stringable(Prefix p) : val(STRR(p)) {}
    explicit(false) Stringable(std::string_view p) : val(p) {}
    explicit(false) Stringable(const std::string &p) : val(p) {}

    explicit(false) operator std::string_view() const { return val; }
};

using Args = std::initializer_list<Stringable>;

static std::string Format(Args args)
{
    // assert(args.size() > 0);

    std::string res;
    for (const auto &a : args)
    {
        res += a;
        res += ':';
    }

    res.pop_back();

    return res;
}

struct KeyNames
{
    using enum Prefix;
   public:
    const std::string coin;
    explicit KeyNames(std::string_view coin) : coin(coin) {}

    const std::string round = Format({coin, EnumName<ROUND>()});
    const std::string round_shares = Format({round, EnumName<SHARES>()});
    const std::string round_efforts = Format({round, EnumName<EFFORT>()});

    const std::string shares = Format({coin, EnumName<SHARES>()});
    const std::string shares_valid = Format({shares, EnumName<VALID>()});
    const std::string shares_stale = Format({shares, EnumName<STALE>()});
    const std::string shares_invalid = Format({shares, EnumName<INVALID>()});

    const std::string hashrate = Format({coin, EnumName<HASHRATE>()});
    const std::string hashrate_average =
        Format({hashrate, EnumName<AVERAGE>()});
    const std::string hashrate_network =
        Format({hashrate, EnumName<NETWORK>()});
    const std::string hashrate_network_compact =
        Format({hashrate_network, EnumName<COMPACT>()});
    const std::string hashrate_pool = Format({hashrate, EnumName<POOL>()});
    const std::string hashrate_pool_compact =
        Format({hashrate_pool, EnumName<COMPACT>()});

    const std::string miner_worker_count = Format({coin, EnumName<SOLVER>(), EnumName<WORKER>(), EnumName<COUNT>()});
    const std::string worker_count = Format({coin, EnumName<WORKER_COUNT>()});
    const std::string worker_count_pool =
        Format({worker_count, EnumName<POOL>()});
    const std::string worker_countp_compact =
        Format({worker_count_pool, EnumName<COMPACT>()});

    const std::string solver_count =
        Format({coin, EnumName<SOLVER>(), EnumName<COUNT>()});

    const std::string miner_count =
        Format({coin, EnumName<MINER_COUNT>(), EnumName<POOL>()});
    const std::string miner_count_compact =
        Format({miner_count, EnumName<COMPACT>()});

    const std::string difficulty = Format({coin, EnumName<DIFFICULTY>()});
    const std::string difficulty_compact =
        Format({difficulty, EnumName<COMPACT>()});

    const std::string solver = Format({coin, EnumName<SOLVER>()});
    const std::string solver_index = Format({solver, EnumName<INDEX>()});
    const std::string solver_index_mature =
        Format({solver_index, EnumName<MATURE_BALANCE>()});
    const std::string solver_index_worker_count =
        Format({solver_index, EnumName<WORKER_COUNT>()});
    const std::string solver_index_hashrate =
        Format({solver_index, EnumName<HASHRATE>()});
    const std::string solver_index_jointime =
        Format({solver_index, EnumName<START_TIME>()});

    const std::string reward = Format({coin, EnumName<REWARD>()});
    const std::string reward_immature = Format({reward, EnumName<IMMATURE>()});
    const std::string reward_mature = Format({reward, EnumName<MATURE>()});

    const std::string address_id_map =
        Format({coin, EnumName<ADDRESS_ID_MAP>()});

    const std::string active_ids_map = Format({coin, EnumName<ACTIVE_IDS>()});
    const std::string payout = Format({coin, EnumName<PAYOUT>()});
    const std::string pending_payout = Format({payout, EnumName<PENDING>()});
};

struct BlockKeyNames{
    using enum Prefix;

    const std::string coin;

    explicit BlockKeyNames(std::string_view coin) : coin(coin) {}

    const std::string block = Format({coin, EnumName<BLOCK>()});
    const std::string block_number = Format({block, EnumName<NUMBER>()});

    const std::string block_effort_percent =
        Format({block, EnumName<EFFORT_PERCENT>()});
    const std::string block_effort_percent_compact =
        Format({block_effort_percent, EnumName<COMPACT>()});
    const std::string mined_block_number =
        Format({coin, EnumName<MINED_BLOCK>(), EnumName<NUMBER>()});
    const std::string mined_block_number_compact =
        Format({coin, EnumName<COMPACT>()});

    // derived
    const std::string block_index = Format({block, EnumName<INDEX>()});
    const std::string block_index_number =
        Format({block_index, EnumName<NUMBER>()});
    const std::string block_index_reward =
        Format({block_index, EnumName<REWARD>()});
    const std::string block_index_difficulty =
        Format({block_index, EnumName<DIFFICULTY>()});
    const std::string block_index_effort =
        Format({block_index, EnumName<EFFORT>()});
    const std::string block_index_duration =
        Format({block_index, EnumName<DURATION>()});
    const std::string block_index_chain =
        Format({block_index, EnumName<CHAIN>()});
    const std::string block_index_solver =
        Format({block_index, EnumName<SOLVER>()});
        
    const std::string block_mature_channel =
        Format({block, EnumName<MATURE>()});
};
#endif