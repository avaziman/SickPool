#ifndef MYSQL_MANAGER_HPP_
#define MYSQL_MANAGER_HPP_
#include <cppconn/connection.h>
#include <cppconn/driver.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/statement.h>
#include <fmt/core.h>
#include <memory>
#include "stats.hpp"
#include <mutex>

#define PRINT_MYSQL_ERR(e) logger.Log<LogType::Critical>("Failed to mysql: {}", e.what())

#include "coin_config.hpp"
#include "redis_interop.hpp"
#include "utils/hex_utils.hpp"
class MySqlManager
{
   private:
    static sql::Driver *driver;
    static std::unique_ptr<sql::Connection> con;

    static std::unique_ptr<sql::PreparedStatement> add_block;
    static std::unique_ptr<sql::PreparedStatement> add_miner;
    static std::unique_ptr<sql::PreparedStatement> get_miner;

    static std::unique_ptr<sql::PreparedStatement> add_worker;
    static std::unique_ptr<sql::PreparedStatement> get_worker;
    static constexpr std::string_view logger_field = "MySQL";
    static const Logger<logger_field> logger;

    static std::mutex mutex;

   public:
    explicit MySqlManager(const CoinConfig& cc);
    void AddBlockSubmission(const BlockSubmission &submission) const;

    void AddMiner(std::string_view address, std::string_view alias,
                  uint64_t min_payout) const;
    int GetMinerId(std::string_view address, std::string_view alias) const;

    void AddWorker(MinerId minerid, std::string_view worker_name) const;
    int GetWorkerId(MinerId minerid, std::string_view worker_name) const;
};

#endif