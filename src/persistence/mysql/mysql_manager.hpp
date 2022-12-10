#ifndef MYSQL_MANAGER_HPP_
#define MYSQL_MANAGER_HPP_
#include <cppconn/connection.h>
#include <cppconn/driver.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/statement.h>
#include <fmt/core.h>
#include <memory>

#include "coin_config.hpp"
#include "redis_interop.hpp"
#include "utils/hex_utils.hpp"
class MySqlManager
{
   private:
    static sql::Driver *driver;
    static std::unique_ptr<sql::Connection> con;

    static std::unique_ptr<sql::PreparedStatement> add_block;
    static constexpr std::string_view logger_field = "MySQL";
    static const Logger<logger_field> logger;

   public:
    explicit MySqlManager(const CoinConfig& cc);
    void AppendAddBlockSubmission(const BlockSubmission &submission) const;
};

#endif