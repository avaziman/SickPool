#ifndef REDIS_BLOCK_HPP
#define REDIS_BLOCK_HPP

#include "redis_block.hpp"

using enum Prefix;
PersistenceBlock::PersistenceBlock(const PersistenceLayer &pl)
    : PersistenceLayer(pl), block_key_names(this->key_names.coin)
{
    using namespace std::string_view_literals;

    const uint64_t mined_blocks_interval_ms =
        conf->stats.mined_blocks_interval * 1000;
    auto pipe =redis->pipeline(false);

    // mined blocks & effort percent
    for (const auto &[key_name, key_compact_name, aggregation] :
         {std::make_tuple(block_key_names.mined_block_number,
                          block_key_names.mined_block_number_compact, "SUM"sv),
          std::make_tuple(block_key_names.block_effort_percent,
                          block_key_names.block_effort_percent_compact,
                          "AVG"sv)})
    {
        TimeSeries ts{mined_blocks_interval_ms, {}, DuplicatePolicy::BLOCK};
        AppendTsCreate(pipe, key_name, ts);

        // X30 duration
        TimeSeries ts_compact{mined_blocks_interval_ms * 30, {}, DuplicatePolicy::BLOCK};
        AppendTsCreate(pipe, key_compact_name, ts_compact);

        pipe.command("TS.CREATERULE"sv, key_name, key_compact_name,
                       "AGGREGATION"sv, aggregation,
                       std::to_string(mined_blocks_interval_ms));
    }

    pipe.exec();
    // if (!GetReplies())
    // {
    //     throw std::invalid_argument(
    //         "Failed to connect to add blocks timeserieses");
    // }
}

void PersistenceBlock::AppendUpdateBlockHeight(sw::redis::Pipeline &pipe, uint32_t number)
{
    pipe.command(block_key_names.block_stats, EnumName<HEIGHT>(),
               std::to_string(number));
}

// bool PersistenceBlock::UpdateImmatureRewards(uint32_t id,s
//                                              int64_t matured_time, bool matured)
// {
    //         // for payment manager...
    //         pipe.publish(block_key_names.block_mature_channel,
    //         "OK");
// }

uint32_t PersistenceBlock::GetBlockHeight()
{
    return *(redis->command<std::optional<long long>>("HGET", block_key_names.block_stats, EnumName<HEIGHT>()));
}
#endif
