#ifndef BLOCK_SUBMITTER_HPP_
#define BLOCK_SUBMITTER_HPP_

#include <mutex>
#include "logger.hpp"
#include "round_manager.hpp"
#include "redis_block.hpp"
#include "block_submission.hpp"


template <Coin coin>
class BlockSubmitter
{
   private:
    static constexpr std::string_view field_str = "BlockSubmitter";
    DaemonManagerT<coin>* daemon_manager;
    RoundManager* round_manager;
    std::mutex blocks_lock;
    Logger logger{field_str};

   public:
    explicit BlockSubmitter(DaemonManagerT<coin>* daemon_manager,
                   RoundManager* round_manager)
        : 
        daemon_manager(daemon_manager),
          round_manager(round_manager)
    {
    }

    inline bool TrySubmit(const std::string_view block_hex,
                          simdjson::ondemand::parser& parser) const
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
    bool AddImmatureBlock(std::unique_ptr<BlockSubmission> submission,
                          const double pow_fee)
    {
        std::scoped_lock lock(blocks_lock);

        // block number increased here.

        if (auto res = round_manager->CloseRound(submission->id,
                                                 *submission.get(), pow_fee);
            res != RoundCloseRes::OK)
        {
            logger.template Log<LogType::Critical>("Failed to close round! {}", static_cast<int>(res));
        }

        logger.template Log<LogType::Info>(
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
            "", fmt::format("Block Submission #{}", submission->id),
            fmt::format("Type: {}", (int)submission->block_type),
            fmt::format("Reward: {}", submission->reward),
            fmt::format("Found time: {}", submission->time_ms),
            fmt::format("Duration (ms): {}", submission->duration_ms),
            fmt::format("Height: {}", submission->height),
            fmt::format("Difficulty: {}", submission->difficulty),
            fmt::format("Effort percent: {}", submission->effort_percent),
            fmt::format("Miner: {}", submission->miner_id),
            fmt::format("Worker: {}", submission->worker_id),
            fmt::format("Hash: {}",
                        HexlifyS(submission->hash_bin)),
            72);

        logger.template Log<LogType::Info>(
            "Closed round for block submission no {} (immature).",
            submission->id);

        return true;
    }
};

#endif