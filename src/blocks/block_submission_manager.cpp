#include "block_submission_manager.hpp"

uint32_t SubmissionManager::block_number;

void SubmissionManager::CheckImmatureSubmissions()
{
    using namespace simdjson;
    std::scoped_lock lock(blocks_lock);

    std::string resBody;

    for (int i = 0; i < immature_block_submissions.size(); i++)
    {
        const auto& submission = immature_block_submissions[i];
        auto hashHex = std::string_view((char*)submission->hashHex,
                                        sizeof(submission->hashHex));
        auto chain = std::string_view((char*)submission->chain,
                                      sizeof(submission->chain));

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
            std::string_view(std::to_string(submission->number)),
            confirmations);

        int submissions_ago = block_number - submission->number;

        int64_t confirmation_time = GetCurrentTimeMs();
        if (confirmations > BLOCK_MATURITY)
        {
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Block {} has matured!", hashHex);

            if (last_matured_time)
            {
                int64_t duration_ms = confirmation_time - last_matured_time;
                redis_manager->AddStakingPoints(chain, duration_ms);
            }

            redis_manager->UpdateImmatureRewards(chain, submissions_ago,
                                                 confirmation_time, true);

            immature_block_submissions.erase(
                immature_block_submissions.begin() + i);

            last_matured_time = confirmation_time;
            i--;
        }
        else if (confirmations == -1)
        {
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Block {} has been orphaned! :(", hashHex);

            redis_manager->UpdateImmatureRewards(chain, submission->number,
                                                 confirmation_time, false);
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
    std::unique_ptr<BlockSubmission> submission, const double pow_fee)
{
    std::scoped_lock lock(blocks_lock);
    redis_manager->IncrBlockCount();
    block_number++;

    if (submission->block_type == BlockType::POW)
    {
        round_manager->ClosePowRound(submission.get(), pow_fee);
    }
    else if (submission->block_type == BlockType::POS)
    {
        round_manager->ClosePosRound(submission.get(), pow_fee);
    }

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
        Logger::Log(LogType::Critical, LogField::SubmissionManager,
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
            Logger::Log(LogType::Critical, LogField::SubmissionManager,
                        "Block submission rejected, rpc error: {}", res);
            return false;
        }

        if (!resultField.is_null())
        {
            std::string_view result = resultField.get_string();
            Logger::Log(LogType::Critical, LogField::SubmissionManager,
                        "Block submission rejected, rpc result: {}", result);

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
        Logger::Log(LogType::Critical, LogField::SubmissionManager,
                    "Submit block response parse error: {}", err.what());
        return false;
    }

    return true;
}

// think of what happens if they deposit at the same time as the block was found