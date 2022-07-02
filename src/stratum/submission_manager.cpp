#include "submission_manager.hpp"

void SubmissionManager::CheckImmatureSubmissions(const double pow_fee)
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

        daemon_manager->SendRpcReq<std::any>(resBody, 1, "getblockheader",
                                             std::any(hashHex));

        int32_t confirmations = -1;
        try
        {
            ondemand::document doc = httpParser.iterate(
                resBody.data(), resBody.size(), resBody.capacity());

            confirmations = (int32_t)doc["result"]["confirmations"].get_int64();
        }
        catch (const simdjson_error& err)
        {
            Logger::Log(
                LogType::Info, LogField::Stratum,
                "Failed to get confirmations for block %.*s, parse error: %s",
                hashHex.size(), hashHex.data(), err.what());
            continue;
        }

        redis_manager->UpdateBlockConfirmations(
            std::string_view(std::to_string(submission.number)), confirmations);

        if (confirmations > 100)
        {
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Block %.*s has matured!", hashHex.size(),
                        hashHex.data());

            stats_manager->ClosePoWRound(chain, submission, pow_fee);

            immature_block_submissions.erase(
                immature_block_submissions.begin() + i);
            i--;
        }
        else if (confirmations == -1)
        {
            Logger::Log(LogType::Info, LogField::Stratum,
                        "Block %.*s has been orphaned! :(", hashHex.size(),
                        hashHex.data());
            immature_block_submissions.erase(
                immature_block_submissions.begin() + i);
            i--;
        }
        Logger::Log(LogType::Info, LogField::Stratum,
                    "Block %.*s has %d confirmations", hashHex.size(),
                    hashHex.data(), confirmations);
    }
}

// TODO: LOCKS
bool SubmissionManager::AddImmatureBlock(std::unique_ptr<BlockSubmission> submission)
{
    block_number++;
    redis_manager->IncrBlockCount();

    std::string block_id_str = std::to_string(block_number);

    redis_manager->AddBlockSubmission(*submission.get(), block_id_str.c_str());

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
                    "Failed to send block submission, http code: %d, res: %.*s",
                    resCode, resultBody.size(), resultBody.data());
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
            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Block submission rejected, rpc error: %s",
                        errorField.get_raw_json_string());
            return false;
        }

        if (!resultField.is_null())
        {
            std::string_view result = resultField.get_string();
            Logger::Log(LogType::Critical, LogField::Stratum,
                        "Block submission rejected, rpc result: %.*s",
                        result.size(), result.data());

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
                    "Submit block response parse error: %s", err.what());
        return false;
    }

    return true;
}
