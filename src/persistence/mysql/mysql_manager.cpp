#include "mysql_manager.hpp"

std::mutex MySqlManager::mutex;

sql::Driver* MySqlManager::driver = get_driver_instance();
std::unique_ptr<sql::Connection> MySqlManager::con;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_block;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_miner;
std::unique_ptr<sql::PreparedStatement> MySqlManager::get_miner;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_worker;
std::unique_ptr<sql::PreparedStatement> MySqlManager::get_worker;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_reward;
std::unique_ptr<sql::PreparedStatement> MySqlManager::get_last_id;
std::unique_ptr<sql::PreparedStatement> MySqlManager::get_immature_blocks;
std::unique_ptr<sql::PreparedStatement> MySqlManager::get_unpaid_rewards;
std::unique_ptr<sql::PreparedStatement> MySqlManager::update_block_status;
std::unique_ptr<sql::PreparedStatement> MySqlManager::update_rewards;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_payout;
std::unique_ptr<sql::PreparedStatement> MySqlManager::add_payout_entry;
std::unique_ptr<sql::PreparedStatement> MySqlManager::update_next_payout;

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
            con->prepareStatement("CALL AddMiner(?,?,?,?)"));

        get_miner = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL GetMiner(?,?)"));

        add_worker = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL AddWorker(?,?,?)"));

        get_worker = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL GetWorker(?,?)"));

        add_reward = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL AddReward(?,?,?,?)"));

        get_last_id = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("SELECT LAST_INSERT_ID()"));

        get_immature_blocks =
            std::unique_ptr<sql::PreparedStatement>(con->prepareStatement(
                "SELECT id,height,status,hash FROM blocks WHERE status=1 OR "
                "status=5"));  // pending orphaned

        get_unpaid_rewards = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL GetUnpaidRewards(?)"));

        update_block_status = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("UPDATE blocks SET status=? WHERE id=?"));

        update_rewards = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL MatureRewards(?,?)"));

        add_payout =
            std::unique_ptr<sql::PreparedStatement>(con->prepareStatement(
                "CALL AddPayout(?,?,?,?,?)"));

        add_payout_entry = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("CALL AddPayoutEntry(?,?,?,?)"));

        update_next_payout = std::unique_ptr<sql::PreparedStatement>(
            con->prepareStatement("UPDATE payout_stats SET next_ms=?"));
    }
    catch (const sql::SQLException& e)
    {
        throw std::invalid_argument(
            fmt::format("Failed to connect to mysql: {}", e.what()));
    }
}

bool MySqlManager::AddBlockSubmission(uint32_t& id,
                                      const BlockSubmission& submission) const
{
    std::scoped_lock _(mutex);
    std::unique_ptr<sql::ResultSet> res;

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

    return GetLastId(id);
}

// not mutexed as it should be called inside mutexed func
bool MySqlManager::GetLastId(uint32_t& id) const
{
    std::unique_ptr<sql::ResultSet> res;

    try
    {
        do
        {
            res = std::unique_ptr<sql::ResultSet>(get_last_id->executeQuery());

            while (res->next())
            {
                id = res->getUInt(1);
            }
        } while (get_last_id->getMoreResults());
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
        return false;
    }

    return true;
}

int64_t MySqlManager::AddMiner(std::string_view address, std::string_view alias,
                            uint64_t join_time, uint64_t min_payout) const
{
    std::unique_lock lock(mutex);

    add_miner->setString(1, std::string(address));

    if (!alias.empty())
    {
        add_miner->setString(2, std::string(alias));
    }
    else
    {
        add_miner->setNull(2, sql::DataType::CHAR);
    }

    add_miner->setUInt64(3, min_payout);
    add_miner->setUInt64(4, join_time);

    try
    {
        /* int affected = */ add_miner->executeUpdate();
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
        return -1;
    }
    lock.unlock();
    return GetMinerId(address, alias);
}

int64_t MySqlManager::GetMinerId(std::string_view address,
                                 std::string_view alias) const
{
    std::scoped_lock _(mutex);
    std::unique_ptr<sql::ResultSet> res;
    int64_t id = -1;

    get_miner->setString(1, std::string(address));
    if (!alias.empty())
    {
        get_miner->setString(2, std::string(alias));
    }
    else
    {
        get_miner->setNull(2, sql::DataType::CHAR);
    }

    try
    {
        do
        {
            res = std::unique_ptr<sql::ResultSet>(get_miner->executeQuery());

            while (res->next())
            {
                id = res->getUInt(1);
            }
        } while (get_miner->getMoreResults());
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
    }

    return id;
}

int64_t MySqlManager::AddWorker(MinerId minerid, std::string_view worker_name,
                             uint64_t join_time) const
{
    std::unique_lock lock(mutex);

    add_worker->setUInt(1, minerid);
    add_worker->setString(2, std::string(worker_name));
    add_worker->setUInt64(3, join_time);

    try
    {
        add_worker->executeUpdate();
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
        return -1;
    }

    lock.unlock();
    return GetWorkerId(minerid, worker_name);
}

int64_t MySqlManager::GetWorkerId(MinerId minerid,
                                  std::string_view worker_name) const
{
    std::scoped_lock _(mutex);
    std::unique_ptr<sql::ResultSet> res;
    int64_t id = -1;

    get_worker->setUInt(1, minerid);
    get_worker->setString(2, std::string(worker_name));

    try
    {
        do
        {
            res = std::unique_ptr<sql::ResultSet>(get_worker->executeQuery());

            while (res->next())
            {
                id = res->getInt(1);
            }
        } while (get_worker->getMoreResults());
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
    }

    return id;
}

bool MySqlManager::AddRoundRewards(const BlockSubmission& submission,
                                   const round_shares_t& miner_shares) const
{
    for (const auto& [miner_id, reward] : miner_shares)
    {
        add_reward->setUInt(1, miner_id);
        add_reward->setUInt(2, submission.id);
        add_reward->setUInt64(3, reward.reward);
        add_reward->setDouble(4, reward.effort);

        try
        {
            add_reward->executeUpdate();
        }
        catch (const sql::SQLException e)
        {
            PRINT_MYSQL_ERR(e);
            return false;
        }
    }
    return true;
}

bool MySqlManager::LoadImmatureBlocks(
    std::vector<BlockOverview>& submissions) const
{
    std::scoped_lock _(mutex);
    std::unique_ptr<sql::ResultSet> res;

    try
    {
        do
        {
            res = std::unique_ptr<sql::ResultSet>(
                get_immature_blocks->executeQuery());

            while (res->next())
            {
                submissions.emplace_back(
                    res->getUInt(1), res->getUInt(2),
                    static_cast<BlockStatus>(res->getInt(3)),
                    res->getString(4));
            }
        } while (get_immature_blocks->getMoreResults());
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
        return false;
    }
    return true;
}

// TODO: perhaps make insert miner use  last id too

bool MySqlManager::UpdateBlockStatus(uint32_t block_id,
                                     BlockStatus status) const
{
    std::scoped_lock _(mutex);

    update_block_status->setInt(1, static_cast<uint8_t>(status));
    update_block_status->setUInt(2, block_id);

    try
    {
        /* int affected = */ update_block_status->executeUpdate();
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
    }

    return true;
}

bool MySqlManager::UpdateImmatureRewards(
    uint32_t block_id, BlockStatus status,
    [[maybe_unused]] int64_t matured_time) const
{
    std::scoped_lock _(mutex);

    update_rewards->setUInt(1, block_id);
    update_rewards->setInt(2, static_cast<uint8_t>(status));

    try
    {
        /* int affected = */ update_rewards->executeUpdate();
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
    }

    return true;
}

bool MySqlManager::UpdateNextPayout(
    uint64_t next_ms) const
{
    std::scoped_lock _(mutex);

    update_next_payout->setUInt64(1, next_ms);

    try
    {
        /* int affected = */ update_next_payout->executeUpdate();
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
    }

    return true;
}

bool MySqlManager::LoadUnpaidRewards(std::vector<Payee>& rewards, uint64_t minimum) const
{
    std::scoped_lock _(mutex);
    std::unique_ptr<sql::ResultSet> res;

    get_unpaid_rewards->setUInt64(1, minimum);
    try
    {
        do
        {
            res = std::unique_ptr<sql::ResultSet>(
                get_unpaid_rewards->executeQuery());

            while (res->next())
            {
                rewards.emplace_back(res->getUInt(1), res->getUInt64(2),
                                     res->getString(3));
            }
        } while (get_unpaid_rewards->getMoreResults());
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
        return false;
    }
    return true;
}

bool MySqlManager::AddPayout(PayoutInfo& pinfo,
                             const std::vector<Payee>& payees,
                             uint64_t individual_fee) const
{
    std::scoped_lock _(mutex);

    add_payout->setString(1, pinfo.txid);
    add_payout->setUInt(2, static_cast<uint32_t>(payees.size()));
    add_payout->setUInt64(3, pinfo.total);
    add_payout->setUInt64(4, pinfo.tx_fee);
    add_payout->setUInt64(5, pinfo.time);

    try
    {
        /* int affected = */ add_payout->executeUpdate();
    }
    catch (const sql::SQLException e)
    {
        PRINT_MYSQL_ERR(e);
        return false;
    }

    if (!GetLastId(pinfo.id)) return false;

    for (const auto& payee : payees)
    {
        add_payout_entry->setUInt(1, pinfo.id);
        add_payout_entry->setUInt(2, payee.miner_id);
        add_payout_entry->setUInt64(3, payee.amount_clean);
        add_payout_entry->setUInt64(4, individual_fee);

        try
        {
            /* int affected = */ add_payout_entry->executeUpdate();
        }
        catch (const sql::SQLException e)
        {
            PRINT_MYSQL_ERR(e);
            return false;
        }
    }

    return true;
}