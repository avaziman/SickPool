#ifndef MYSQL_MANAGER_HPP_
#define MYSQL_MANAGER_HPP_
#include <cppconn/connection.h>
#include <cppconn/driver.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/statement.h>
#include <fmt/core.h>

#include <memory>
#include <mutex>

#include "block_submission.hpp"
#include "coin_config.hpp"
#include "redis_interop.hpp"
#include "redis_manager.hpp"
#include "round_share.hpp"
#include "stats.hpp"
#include "utils/hex_utils.hpp"

#define PRINT_MYSQL_ERR(e) \
    logger.Log<LogType::Critical>("Failed to mysql: {}", e.what())

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
    static std::unique_ptr<sql::PreparedStatement> add_reward;
    static std::unique_ptr<sql::PreparedStatement> get_last_id;
    static std::unique_ptr<sql::PreparedStatement> get_immature_blocks;
    static std::unique_ptr<sql::PreparedStatement> get_unpaid_rewards;
    static std::unique_ptr<sql::PreparedStatement> update_block_status;
    static std::unique_ptr<sql::PreparedStatement> update_rewards;
    static std::unique_ptr<sql::PreparedStatement> add_payout;
    static std::unique_ptr<sql::PreparedStatement> add_payout_entry;
    static std::unique_ptr<sql::PreparedStatement> update_next_payout;

    static constexpr std::string_view logger_field = "MySQL";
    static const Logger<logger_field> logger;

    static std::mutex mutex;

   public:
    explicit MySqlManager(const CoinConfig &cc);
    bool AddBlockSubmission(uint32_t &id,
                            const BlockSubmission &submission) const;

    int64_t AddMiner(std::string_view address, std::string_view alias,
                  uint64_t join_time, uint64_t min_payout) const;
    int64_t GetMinerId(std::string_view address, std::string_view alias) const;

    int64_t AddWorker(MinerId minerid, std::string_view worker_name,
                   uint64_t join_time) const;
    int64_t GetWorkerId(MinerId minerid, std::string_view worker_name) const;

    bool AddRoundRewards(const BlockSubmission &submission,
                         const round_shares_t &miner_shares) const;

    bool GetLastId(uint32_t& id) const;
    bool LoadUnpaidRewards(std::vector<Payee> &rewards, uint64_t minimum) const;
    bool LoadImmatureBlocks(std::vector<BlockOverview> &submissions) const;
    bool UpdateBlockStatus(uint32_t block_id, BlockStatus status) const;
    bool UpdateImmatureRewards(uint32_t block_num, BlockStatus status,
                               int64_t matured_time) const;

    bool UpdateNextPayout(uint64_t next_ms) const ;
    bool AddPayout(PayoutInfo &pinfo, const std::vector<Payee> &payees,
                   uint64_t amount_clean) const;
};

#endif