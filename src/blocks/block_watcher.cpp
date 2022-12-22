#include "block_watcher.hpp"
template class BlockWatcher<ZanoStatic>;

template <StaticConf confs>
BlockWatcher<confs>::BlockWatcher(const PersistenceLayer* pl,
                                  daemon_manager_t* daemon_manager)
    : persistence_block(*pl), daemon_manager(daemon_manager)
{
}

template <StaticConf confs>
void BlockWatcher<confs>::CheckImmatureSubmissions()
{
    using namespace simdjson;
    std::scoped_lock lock(blocks_lock);
    uint32_t current_height = persistence_block.GetBlockHeight();
    std::string resBody;

    persistence_block.LoadImmatureBlocks(immature_block_submissions);

    logger.template Log<LogType::Info>("Current height: {}, checking {} blocks",
                                       current_height,
                                       immature_block_submissions.size());

    for (auto it = immature_block_submissions.begin();
         it != immature_block_submissions.end();)
    {
        const auto& submission = *it;
        uint8_t chain = 0;

        logger.template Log<LogType::Info>(
            "checking block submission id: {}, hash: {}", submission.id,
            submission.hash);

        BlockHeaderResCn header_res;
        int confirmations = 0;

        if (bool res = daemon_manager->GetBlockHeaderByHeight(
                header_res, submission.height, httpParser);
            !res)
        {
            logger.template Log<LogType::Info>(
                "Failed to get confirmations for block {}", submission.hash);
        }
        else if (submission.hash == header_res.hash)
        {
            confirmations = header_res.depth;
        }
        else
        {
            confirmations = -1;
        }

        logger.template Log<LogType::Info>("Block {} has {} confirmations",
                                           submission.hash, confirmations);

        // 100% orphaned or matured
        if (current_height > submission.height + confs.COINBASE_MATURITY)
        {
            int64_t confirmation_time = GetCurrentTimeMs();
            BlockStatus status = confirmations > static_cast<int>(confs.COINBASE_MATURITY)
                                     ? BlockStatus::CONFIRMED
                                     : BlockStatus::ORPHANED;

            persistence_block.UpdateImmatureRewards(submission.id, status,
                                                    confirmation_time);
            persistence_block.UpdateBlockStatus(submission.id, status);

            logger.template Log<LogType::Info>(
                "Block {} has been {}!", submission.hash,
                status == BlockStatus::CONFIRMED
                    ? EnumName<BlockStatus::CONFIRMED>()
                    : EnumName<BlockStatus::ORPHANED>());

            it = immature_block_submissions.erase(it);
        }
        else
        {
            if (confirmations == -1 &&
                submission.last_status != BlockStatus::PENDING_ORPHANED)
            {
                persistence_block.UpdateBlockStatus(
                    submission.id, BlockStatus::PENDING_ORPHANED);
            }
            ++it;
        }
    }
}

template <StaticConf confs>
void BlockWatcher<confs>::WatchBlocks()
{
    persistence_block.SubscribeToBlockNotify();
    while (true)
    {
        CheckImmatureSubmissions();

        auto [res, rep] = persistence_block.GetOneReply(0);
    }
}
