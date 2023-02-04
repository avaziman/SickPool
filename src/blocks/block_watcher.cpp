#include "block_watcher.hpp"
template class BlockWatcher<ZanoStatic>;

template <StaticConf confs>
BlockWatcher<confs>::BlockWatcher(const PersistenceLayer* pl,
                                  DaemonManagerT* daemon_manager)
    : persistence_block(*pl), daemon_manager(daemon_manager)
{
}

template <StaticConf confs>
void BlockWatcher<confs>::CheckImmatureSubmissions()
{
    using enum BlockStatus;
    using enum LogType;
    using namespace simdjson;
    std::scoped_lock lock(blocks_lock);
    uint32_t current_height = persistence_block.GetBlockHeight();
    std::string resBody;

    persistence_block.LoadImmatureBlocks(immature_block_submissions);

    logger.template Log<Info>("Current height: {}, checking {} blocks",
                              current_height,
                              immature_block_submissions.size());

    for (auto it = immature_block_submissions.begin();
         it != immature_block_submissions.end();)
    {
        const auto& submission = *it;
        uint8_t chain = 0;

        logger.template Log<Info>("checking block submission id: {}, hash: {}",
                                  submission.id, submission.hash);

        BlockHeaderResCn header_res;
        int confirmations = 0;

        if (bool res = daemon_manager->GetBlockHeaderByHeight(
                header_res, submission.height, httpParser);
            !res)
        {
            logger.template Log<Info>(
                "Failed to get confirmations for block {}, skipping",
                submission.hash);
            ++it;
            continue;
        }
        // the block at this height is indeed the one we submitted
        else if (submission.hash == header_res.hash)
        {
            confirmations = header_res.depth;
        }
        // block has been orphaned as the daemon has other block in its height
        else
        {
            confirmations = -1;
            if (submission.last_status != PENDING_ORPHANED)
            {
                persistence_block.UpdateBlockStatus(submission.id,
                                                    PENDING_ORPHANED);
            }
        }

        logger.template Log<Info>("Block {} has {} confirmations",
                                  submission.hash, confirmations);

        // 100% orphaned or matured
        if (current_height > submission.height + confs.COINBASE_MATURITY)
        {
            int64_t confirmation_time = GetCurrentTimeMs();
            BlockStatus status =
                confirmations > static_cast<int>(confs.COINBASE_MATURITY)
                    ? CONFIRMED
                    : ORPHANED;

            persistence_block.UpdateImmatureRewards(submission.id, status,
                                                    confirmation_time);
            persistence_block.UpdateBlockStatus(submission.id, status);

            logger.template Log<LogType::Info>(
                "Block {} has been {}!", submission.hash,
                status == BlockStatus::CONFIRMED ? EnumName<CONFIRMED>()
                                                 : EnumName<ORPHANED>());

            it = immature_block_submissions.erase(it);
        }
        else
        {
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
