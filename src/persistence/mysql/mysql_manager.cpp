#include "mysql_manager.hpp"

sql::Driver* MySqlManager::driver = get_driver_instance();
std::unique_ptr<sql::Connection> MySqlManager::con;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_block;
const Logger<MySqlManager::logger_field> MySqlManager::logger;

MySqlManager::MySqlManager(const CoinConfig& cc)
{
    try
    {
        con = std::unique_ptr<sql::Connection>(
            driver->connect(cc.mysql.host, cc.mysql.user, cc.mysql.pass));

        if (!con->isValid())
        {
            throw std::invalid_argument("Failed to connect to mysql");
        }

        std::unique_ptr<sql::Statement> stmt(con->createStatement());

        stmt->execute(fmt::format("USE {}", cc.symbol));

        add_block = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL AddBlock(?,?,?,?,?,?,?,?)"));
    }
    catch (const sql::SQLException& e)
    {
        throw std::invalid_argument(
            fmt::format("Failed to connect to mysql: {}", e.what()));
    }
}

void MySqlManager::AppendAddBlockSubmission(
    const BlockSubmission& submission) const
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

    try
    {
        add_block->execute();
    }
    catch (const sql::SQLException e)
    {
        logger.Log<LogType::Critical>("Failed to add block submission: {}",
                                      e.what());
    }
}
