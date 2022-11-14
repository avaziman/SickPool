#ifndef BLOCK_SUBMITTER_HPP_
#define BLOCK_SUBMITTER_HPP_

#include <mutex>

#include "logger.hpp"
#include "round_manager.hpp"

class BlockSubmitter
{
   private:
    RoundManager* round_manager;
    daemon_manager_t* daemon_manager;
    RedisManager* redis_manager;
    static constexpr std::string_view field_str = "BlockSubmitter";
    Logger<field_str> logger;

   public:
    BlockSubmitter(RedisManager* redis_manager,
                   daemon_manager_t* daemon_manager,
                   RoundManager* round_manager)
        : redis_manager(redis_manager),
          daemon_manager(daemon_manager),
          round_manager(round_manager)
    {
    }

    inline bool TrySubmit(const std::string_view chain,
                          const std::string_view block_hex,
                          simdjson::ondemand::parser& parser)
    {
        const int submit_retries = 5;
        bool added = false;
        for (int i = 0; i < submit_retries; ++i)
        {
            added = daemon_manager->SubmitBlock(block_hex, parser);

            if (added)
            {
                break;
            }
        }
        return added;
    }
    std::mutex blocks_lock;
    bool AddImmatureBlock(std::unique_ptr<BlockSubmission> submission,
                          const double pow_fee)
    {
        std::scoped_lock lock(blocks_lock);

        // block number increased here.
        // round_manager->CloseRound(submission.get(), pow_fee);
        round_manager->CloseRound(submission.get(), pow_fee);

        logger.Log<LogType::Info>(
            "Added new block submission: \n"
            "┌{0:─^{12}}┐\n"
            "│{1: ^{12}}│\n"
            "│{2: <{12}}│\n"
            "│{3: <{12}}│\n"
            "│{4: <{12}}│\n"
            "│{5: <{12}}│\n"
            "│{6: <{12}}│\n"
            "│{7: <{12}}│\n"
            "│{8: <{12}}│\n"
            "│{9: <{12}}│\n"
            "│{10: <{12}}│\n"
            "│{11: <{12}}│\n"
            "└{0:─^{12}}┘\n",
            "", fmt::format("Block Submission #{}", submission->number),
            fmt::format("Type: {}", (int)submission->block_type),
            fmt::format("Reward: {}", submission->block_reward),
            fmt::format("Found time: {}", submission->time_ms),
            fmt::format("Duration (ms): {}", submission->duration_ms),
            fmt::format("Height: {}", submission->height),
            fmt::format("Difficulty: {}", submission->difficulty),
            fmt::format("Effort percent: {}", submission->effort_percent),
            fmt::format("Miner: {}", submission->miner_id),
            fmt::format("Worker: {}", submission->worker_id),
            fmt::format("Hash: {}",
                        std::string_view((char*)submission->hash_hex.data(),
                                         sizeof(submission->hash_hex))),
            72);

        logger.Log<LogType::Info>(
            "Closed round for block submission no {} (immature).",
            submission->number);
        // immature_block_submissions.push_back(std::move(submission));

        return true;
    }
};

#endif