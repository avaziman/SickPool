#include "mysql_manager.hpp"

MySqlManager::MySqlManager(const std::string &ip, const CoinConfig *cc)
    : con(std::unique_ptr<sql::Connection>(
          driver->connect(ip, "username", "password")))
{
    add_block = std::unique_ptr<sql::PreparedStatement>(
        con->prepareStatement("CALL AddBlock(?,?,?,?,?,?,?,?)"));
}

void MySqlManager::AppendAddBlockSubmission(const BlockSubmission &submission) const
{
    auto hash_hex = HexlifyS(submission.hash_bin);

    add_block->setUInt(1, submission.worker_id);
    add_block->setString(2, hash_hex);
    add_block->setUInt64(3, submission.reward);
    add_block->setUInt64(4, submission.time_ms);
    add_block->setUInt64(5, submission.duration_ms);
    add_block->setUInt(6, submission.height);
    add_block->setDouble(7, submission.difficulty);
    add_block->setDouble(8, submission.effort_percent);

    add_block->execute();
}
