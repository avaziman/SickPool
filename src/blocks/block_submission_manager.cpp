#include "block_submission_manager.hpp"

void SubmissionManager::CheckImmatureSubmissions()
{
    using namespace simdjson;
    std::string resBody;

    for (int i = 0; i < immature_block_submissions.size(); i++)
    {
        const auto& submission = *immature_block_submissions[i];
        auto hashHex = std::string_view((char*)submission.hashHex,
                                        sizeof(submission.hashHex));
        auto chain =
            std::string_view((char*)submission.chain, sizeof(submission.chain));

        auto res = daemon_manager->SendRpcReq<std::any>(
            resBody, 1, "getblockheader", std::any(hashHex));

        int32_t confirmations = -1;
        try
        {
            ondemand::document doc = httpParser.iterate(
                resBody.data(), resBody.size(), resBody.capacity());

            confirmations = (int32_t)doc["result"]["confirmations"].get_int64();
        }
        catch (const simdjson_error& err)
        {
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Failed to get confirmations for block {}, parse "
                        "error: {}, http code: {}",
                        hashHex, err.what(), res);
            continue;
        }

        redis_manager->UpdateBlockConfirmations(
            std::string_view(std::to_string(submission.number)), confirmations);

        if (confirmations > BLOCK_MATURITY)
        {
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Block {} has matured!", hashHex);
            redis_manager->UpdateImmatureRewards(chain, submission.timeMs,
                                                 true);

            immature_block_submissions.erase(
                immature_block_submissions.begin() + i);
            i--;
        }
        else if (confirmations == -1)
        {
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Block {} has been orphaned! :(", hashHex);

            redis_manager->UpdateImmatureRewards(chain, submission.timeMs,
                                                 false);
            immature_block_submissions.erase(
                immature_block_submissions.begin() + i);
            i--;
        }
        Logger::Log(LogType::Info, LogField::Stratum,
                    "Block {} has {} confirmations", hashHex, confirmations);
    }
}

// TODO: LOCKS
bool SubmissionManager::AddImmatureBlock(
    const std::string_view chainsv, const std::string_view workerFull,
    const job_t* job, const ShareResult& shareRes, const Round& chainRound,
    const int64_t time, const double pow_fee)
{
    // auto submission = std::make_unique<BlockSubmission>(
    //     chainsv, workerFull, job, shareRes, chainRound, time, block_number);
    const int confirmations = 0;
    const int64_t duration = time - chainRound.round_start_ms;
    const double effortPercent = chainRound.pow / job->GetEstimatedShares();

    auto submission = std::make_unique<BlockSubmission>(
        confirmations, job->GetBlockReward(), time, duration, job->GetHeight(),
        block_number, shareRes.Diff, effortPercent);

    memcpy(submission->chain, chainsv.data(), chainsv.size());
    memcpy(submission->miner, workerFull.data(), ADDRESS_LEN);
    memcpy(submission->worker, workerFull.data(), workerFull.size());

    redis_manager->IncrBlockCount();
    block_number++;

    redis_manager->AddBlockSubmission(submission.get());
    Logger::Log(LogType::Info, LogField::SubmissionManager,
                "Block submission no {} added.", submission->number);

    stats_manager->ClosePoWRound(chainsv, submission.get(), pow_fee);

    Logger::Log(LogType::Info, LogField::SubmissionManager,
                "Closed round for block submission no {} (immature).",
                submission->number);

    immature_block_submissions.push_back(std::move(submission));

    return true;
}

bool SubmissionManager::SubmitBlock(std::string_view block_hex)
{
    std::string resultBody;
    int resCode = daemon_manager->SendRpcReq<std::any>(
        resultBody, 1, "submitblock", std::any(block_hex));

    if (resCode != 200)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Failed to send block submission, http code: {}, res: {}",
                    resCode, resultBody);
        return false;
    }

    try
    {
        using namespace simdjson;
        ondemand::document doc = httpParser.iterate(
            resultBody.data(), resultBody.size(), resultBody.capacity());

        ondemand::object res = doc.get_object();
        ondemand::value resultField = res["result"];
        ondemand::value errorField = res["error"];

        if (!errorField.is_null())
        {
            std::string_view res = errorField.get_string();
            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Block submission rejected, rpc error: {}", res);
            return false;
        }

        if (!resultField.is_null())
        {
            std::string_view result = resultField.get_string();
            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Block submission rejected, rpc result: {}",
                        result);

            if (result == "inconclusive")
            {
                Logger::Log(
                    LogType::Warn, LogField::Stratum,
                    "Submitted inconclusive block, waiting for result...");
                return true;
            }
            return false;
        }
    }
    catch (const simdjson::simdjson_error& err)
    {
        Logger::Log(LogType::Critical, LogField::Stratum,
                    "Submit block response parse error: {}", err.what());
        return false;
    }

    return true;
}

// think of what happens if they deposit at the same time as the block was found