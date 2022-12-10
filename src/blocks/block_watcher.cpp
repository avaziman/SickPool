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
    uint32_t current_height;
    std::string resBody;

    persistence_block.LoadImmatureBlocks(immature_block_submissions);

    for (const std::unique_ptr<BlockSubmission>& sub :
         immature_block_submissions)
    {
        logger.template Log<LogType::Info>(
            "Block watcher loaded immature block id: {}, hash: {}", sub->number,
            HexlifyS(sub->hash_bin));
    }

    logger.template Log<LogType::Info>("Checking confirmations for {} blocks",
                                       immature_block_submissions.size());

    for (int i = 0; i < immature_block_submissions.size(); i++)
    {
        const auto& submission = immature_block_submissions[i];
        auto hashHex = HexlifyS(submission->hash_bin);
        auto chain = submission->chain;

        BlockHeaderResCn header_res;
        bool res =
            daemon_manager->GetBlockHeader(header_res, hashHex, httpParser);

        if (!res)
        {
            logger.template Log<LogType::Info>(
                "Failed to get confirmations for block {}", hashHex);
            continue;
        }

        int confirmations = header_res.depth;

        persistence_block.UpdateBlockConfirmations(
            std::string_view(std::to_string(submission->number)),
            confirmations);

        logger.template Log<LogType::Info>("Block {} has {} confirmations",
                                           hashHex, confirmations);

        int64_t confirmation_time = GetCurrentTimeMs();

        // 100% orphaned or matured
        if (current_height > submission->height + confs.COINBASE_MATURITY)
        {
            bool matured = confirmations > confs.COINBASE_MATURITY;

            std::string desc = matured ? "matured" : "been orphaned";
            logger.template Log<LogType::Info>("Block {} has {}!", hashHex,
                                               desc);

            persistence_block.UpdateImmatureRewards(chain, submission->number,
                                                 confirmation_time, matured);
            immature_block_submissions.erase(
                immature_block_submissions.begin() + i);
            i--;
        }
    }
}

template <StaticConf confs>
void BlockWatcher<confs>::WatchBlocks()
{
    while (true)
    {
        auto [res, rep] = persistence_block.GetOneReply(0);

        CheckImmatureSubmissions();
    }
}