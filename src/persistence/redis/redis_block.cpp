#ifndef REDIS_BLOCK_HPP
#define REDIS_BLOCK_HPP

#include "redis_block.hpp"

using enum Prefix;
PersistenceBlock::PersistenceBlock(const PersistenceLayer &pl)
    : PersistenceLayer(pl), block_key_names(this->key_names.coin)
{
    using namespace std::string_view_literals;

    const int64_t mined_blocks_interval_ms =
        conf->stats.mined_blocks_interval * 1000;

    // mined blocks & effort percent
    for (const auto &[key_name, key_compact_name, aggregation] :
         {std::make_tuple(block_key_names.mined_block_number,
                          block_key_names.mined_block_number_compact, "SUM"sv),
          std::make_tuple(block_key_names.block_effort_percent,
                          block_key_names.block_effort_percent_compact,
                          "AVG"sv)})
    {
        AppendCommand({"TS.CREATE"sv, key_name, "RETENTION"sv,
                       std::to_string(mined_blocks_interval_ms)});

        AppendCommand({"TS.CREATE"sv, key_compact_name, "RETENTION"sv,
                       std::to_string(mined_blocks_interval_ms * 30)});

        AppendCommand({"TS.CREATERULE"sv, key_name, key_compact_name,
                       "AGGREGATION"sv, aggregation,
                       std::to_string(mined_blocks_interval_ms)});
    }

    if (!GetReplies())
    {
        throw std::invalid_argument(
            "Failed to connect to add blocks timeserieses");
    }
}

void PersistenceBlock::AppendUpdateBlockHeight(uint32_t number)
{
    AppendHset(block_key_names.block_stats, EnumName<HEIGHT>(),
               std::to_string(number));
}

// bool PersistenceBlock::UpdateImmatureRewards(uint32_t id,
//                                              int64_t matured_time, bool matured)
// {
    //         // for payment manager...
    //         AppendCommand({"PUBLISH", block_key_names.block_mature_channel,
    //         "OK"});
// }

bool PersistenceBlock::SubscribeToMaturityChannel()
{
    Command({"SUBSCRIBE", block_key_names.block_mature_channel});
    return true;
}

bool PersistenceBlock::SubscribeToBlockNotify()
{
    Command({"SUBSCRIBE", block_key_names.block});
    return true;
}

uint32_t PersistenceBlock::GetBlockHeight()
{
    return static_cast<uint32_t>(ResToInt(Command({"HGET", block_key_names.block_stats, EnumName<HEIGHT>()})));
}
#endif
