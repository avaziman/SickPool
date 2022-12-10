#include "mysql_manager.hpp"

std::mutex MySqlManager::mutex;

sql::Driver* MySqlManager::driver = get_driver_instance();
std::unique_ptr<sql::Connection> MySqlManager::con;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_block;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_miner;
std::unique_ptr<sql::PreparedStatement> MySqlManager::get_miner;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_worker;
std::unique_ptr<sql::PreparedStatement> MySqlManager::get_worker;
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
            con->prepareStatement("CALL AddBlock(?,?,?,?,?,?,?,?,?)"));

        add_miner = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL AddMiner(?,?,?)"));

        get_miner = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL GetMiner(?,?)"));

        add_worker = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL AddWorker(?,?)"));

        get_worker = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL GetWorker(?,?)"));
    }
    catch (const sql::SQLException& e)
    {
        throw std::invalid_argument(
            fmt::format("Failed to connect to mysql: {}", e.what()));
    }
}

void MySqlManager::AddBlockSubmission(const BlockSubmission& submission) const
{
    std::scoped_lock _(mutex);
    auto hash_hex = HexlifyS(submission.hash_bin);

    add_block->setUInt(1, submission.worker_id);
    add_block->setUInt(2, submission.miner_id);
    add_block->setString(3, hash_hex);
    add_block->setUInt64(4, submission.reward);
    add_block->setUInt64(5, submission.time_ms);
    add_block->setUInt64(6, submission.duration_ms);
    add_block->setUInt(7, submission.height);
    add_block->setDouble(8, submission.difficulty);
    add_block->setDouble(9, submission.effort_percent);

    try
    {
        add_block->execute();
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
    }
    add_block->close();
}

void MySqlManager::AddMiner(std::string_view address, std::string_view alias,
                            uint64_t min_payout) const
{
    std::scoped_lock _(mutex);

    add_miner->setString(1, std::string(address));
    add_miner->setString(2, std::string(alias));
    add_miner->setUInt64(3, min_payout);

    try
    {
        int affected = add_miner->executeUpdate();
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
    }
}

int MySqlManager::GetMinerId(std::string_view address,
                             std::string_view alias) const
{
    std::scoped_lock _(mutex);
    std::unique_ptr<sql::ResultSet> res;
    int id = -1;

    get_miner->setString(1, std::string(address));
    get_miner->setString(2, std::string(alias));

    do
    {
        try
        {
            res = std::unique_ptr<sql::ResultSet>(get_miner->executeQuery());

            while (res->next())
            {
                id = res->getInt(1);
            }
        }
        catch (const sql::SQLException e)
        {
            PRINT_MYSQL_ERR(e);
        }
    } while (get_miner->getMoreResults());

    // res->close();
    return id;
}

void MySqlManager::AddWorker(MinerId minerid,
                             std::string_view worker_name) const
{
    std::scoped_lock _(mutex);

    add_worker->setUInt(1, minerid);
    add_worker->setString(2, std::string(worker_name));

    try
    {
        add_worker->executeUpdate();
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
    }
}

int MySqlManager::GetWorkerId(MinerId minerid,
                              std::string_view worker_name) const
{
    std::scoped_lock _(mutex);
    std::unique_ptr<sql::ResultSet> res;
    int id = -1;

    get_worker->setUInt(1, minerid);
    get_worker->setString(2, std::string(worker_name));

    try
    {
        res = std::unique_ptr<sql::ResultSet>(get_worker->executeQuery());

        res->next();
        id = res->getInt(1);
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
    }

    res->close();
    return id;
}