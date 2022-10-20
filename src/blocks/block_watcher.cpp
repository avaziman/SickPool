#include "block_watcher.hpp"

void BlockWatcher::CheckImmatureSubmissions()
{
    using namespace simdjson;
    std::scoped_lock lock(blocks_lock);
    uint32_t current_height;
    std::string resBody;

    logger.Log<LogType::Info>("Checking confirmations for {} blocks",
                              immature_block_submissions.size());

    for (int i = 0; i < immature_block_submissions.size(); i++)
    {
        const auto& submission = immature_block_submissions[i];
        auto hashHex = std::string_view((char*)submission->hash_hex,
                                        sizeof(submission->hash_hex));
        auto chain = submission->chain_sv;

        int res = daemon_manager->SendRpcReq(resBody, 1, "getblockheader",
                                             DaemonRpc::GetParamsStr(hashHex));

        if (res != 200)
        {
            logger.Log<LogType::Info>(
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
            logger.Log<LogType::Info>(
                "Failed to get confirmations for block {}, parse "
                "error: {}",
                hashHex, err.what());
            continue;
        }

        redis_manager->UpdateBlockConfirmations(
            std::string_view(std::to_string(submission->number)),
            confirmations);

        logger.Log<LogType::Info>("Block {} has {} confirmations", hashHex,
                                  confirmations);

        int64_t confirmation_time = GetCurrentTimeMs();

        // 100% orphaned
        if (confirmations == -1 &&
            current_height > submission->height + COINBASE_MATURITY)
        {
            logger.Log<LogType::Info>("Block {} has been orphaned! :(",
                                      hashHex);

            redis_manager->UpdateImmatureRewards(chain, submission->number,
                                                 confirmation_time, false);
            immature_block_submissions.erase(
                immature_block_submissions.begin() + i);
            i--;
        }
        else if (confirmations > COINBASE_MATURITY)
        {
            logger.Log<LogType::Info>("Block {} has matured!", hashHex);

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