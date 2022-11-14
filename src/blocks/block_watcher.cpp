#include "block_watcher.hpp"

template <StaticConf confs>
BlockWatcher<confs>::BlockWatcher(RedisManager* redis_manager,
                                  daemon_manager_t* daemon_manager)
    : redis_manager(redis_manager), daemon_manager(daemon_manager)
{
    redis_manager->LoadImmatureBlocks(immature_block_submissions);

    for (const std::unique_ptr<BlockSubmission>& sub :
         immature_block_submissions)
    {
        logger.template Log<LogType::Info>(
            "Block watcher loaded immature block id: {}, hash: {}", sub->number,
            std::string_view(sub->hash_hex.data(), confs.PREVHASH_SIZE * 2));
    }
}

template <StaticConf confs>
void BlockWatcher<confs>::CheckImmatureSubmissions()
{
    using namespace simdjson;
    std::scoped_lock lock(blocks_lock);
    uint32_t current_height;
    std::string resBody;

    logger.template Log<LogType::Info>("Checking confirmations for {} blocks",
                                       immature_block_submissions.size());

    for (int i = 0; i < immature_block_submissions.size(); i++)
    {
        const auto& submission = immature_block_submissions[i];
        auto hashHex = std::string_view((char*)submission->hash_hex,
                                        sizeof(submission->hash_hex));
        auto chain = submission->chain;

        int res = daemon_manager->SendRpcReq(resBody, 1, "getblockheader",
                                             DaemonRpc::GetParamsStr(hashHex));

        if (res != 200)
        {
            logger.template Log<LogType::Info>(
                "Failed to get confirmations for block {}, parse "
                "error: {}, http code: {}",
                hashHex, resBody, res);
            continue;
        }

        int32_t confirmations = -1;
        try
        {
            ondemand::document doc = httpParser.iterate(
                resBody.data(), resBody.size(), resBody.capacity());

            confirmations = (int32_t)doc["result"]["confirmations"].get_int64();
        }
        catch (const simdjson_error& err)
        {
            confirmations = 0;
            logger.template Log<LogType::Info>(
                "Failed to get confirmations for block {}, parse "
                "error: {}",
                hashHex, err.what());
            continue;
        }

        redis_manager->UpdateBlockConfirmations(
            std::string_view(std::to_string(submission->number)),
            confirmations);

        logger.template Log<LogType::Info>("Block {} has {} confirmations",
                                           hashHex, confirmations);

        int64_t confirmation_time = GetCurrentTimeMs();

        // 100% orphaned
        if (confirmations == -1 &&
            current_height > submission->height + confs.COINBASE_MATURITY)
        {
            logger.template Log<LogType::Info>("Block {} has been orphaned! :(",
                                               hashHex);

            redis_manager->UpdateImmatureRewards(chain, submission->number,
                                                 confirmation_time, false);
            immature_block_submissions.erase(
                immature_block_submissions.begin() + i);
            i--;
        }
        else if (confirmations > confs.COINBASE_MATURITY)
        {
            logger.template Log<LogType::Info>("Block {} has matured!",
                                               hashHex);

            //     int64_t duration_ms = confirmation_time - last_matured_time;
            //     redis_manager->AddStakingPoints(chain, duration_ms);
            // }

            redis_manager->UpdateImmatureRewards(chain, submission->number,
                                                 confirmation_time, true);

            immature_block_submissions.erase(
                immature_block_submissions.begin() + i);

            i--;
        }
    }
}

// think of what happens if they deposit at the same time as the block was found