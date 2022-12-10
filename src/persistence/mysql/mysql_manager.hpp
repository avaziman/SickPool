#ifndef MYSQL_MANAGER_HPP_
#include <cppconn/statement.h>
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>

#include <memory>
#include "utils/hex_utils.hpp"
#include "coin_config.hpp"
#include "redis_interop.hpp"
class MySqlManager
{
    sql::Driver *driver =
        get_driver_instance();
    std::unique_ptr<sql::Connection> con;

    std::unique_ptr<sql::PreparedStatement> add_block;

    explicit MySqlManager(const std::string &ip, const CoinConfig *cc);
    void AppendAddBlockSubmission(const BlockSubmission &submission) const;
};

#endif