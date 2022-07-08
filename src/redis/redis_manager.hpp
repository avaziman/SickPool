#ifndef REDIS_MANAGER_HPP_
#define REDIS_MANAGER_HPP_
#include <fmt/format.h>
#include <hiredis/hiredis.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "redis_transactoin.hpp"
#include "logger.hpp"
#include "stats/stats.hpp"
#include "stats/stats_manager.hpp"
#include "shares/share.hpp"
#include "blocks/block_submission.hpp"
#include "static_config/config.hpp"

#define xstr(s) str(s)
#define str(s) #s

// how we store stale and invalid shares in database
class RedisManager
{
   public:
    RedisManager(std::string ip, int port);

    bool UpdateBlockConfirmations(std::string_view block_id,
                                  int32_t confirmations);

    bool AddBlockSubmission(const BlockSubmission *submission);
    bool SetEstimatedNeededEffort(std::string_view chain, double effort);

    void ClosePoSRound(int64_t roundStartMs, int64_t foundTimeMs,
                       int64_t reward, uint32_t height,
                       const double totalEffort, const double fee);

    bool DoesAddressExist(std::string_view addrOrId, std::string &valid_addr);

    int64_t GetLastRoundTimePow();
    bool SetLastRoundTimePow(std::string_view chain, int64_t time);

    uint32_t GetBlockNumber();

    bool IncrBlockCount();

    int AddNetworkHr(std::string_view chain, int64_t time, double hr);

    void UpdatePoS(uint64_t from, uint64_t maturity);
    bool UpdateImmatureRewards(std::string_view chain, int64_t rewardTime,
                               bool matured);

    bool AddWorker(std::string_view address, std::string_view worker_full,
                   std::string_view idTag, int64_t curtime, bool newWorker,
                   bool newMiner);

    bool PopWorker(std::string_view address);

    int hgeti(std::string_view key, std::string_view field);

    double hgetd(std::string_view key, std::string_view field);

    bool LoadSolvers(miner_map &miner_stats_map,
                     round_map &round_map);
    bool ClosePoWRound(
        std::string_view chain, const BlockSubmission *submission, double fee,
        miner_map
            &miner_stats_map,
        round_map
            &round_map);

   private:
    redisContext *rc;
    std::mutex rc_mutex;
    std::string coin_symbol;

    bool hset(std::string_view key, std::string_view field,
              std::string_view val);
    int AppendUpdateWorkerCount(std::string_view address, int amount);
    int AppendCreateStatsTs(std::string_view addrOrWorker, std::string_view id,
                            std::string_view prefix);
    int AppendTsCreate(
        std::string_view key_name, int retention,
        std::initializer_list<std::tuple<std::string_view, std::string_view>>
            labels);

    int AppendTsAdd(std::string_view key_name, int64_t time, double value);

    int AppendStatsUpdate(std::string_view addr, std::string_view prefix,
                          int64_t update_time_ms, double hr,
                          const WorkerStats &ws);
};

#endif
// TODO: onwalletnotify check if its pending block submission maybe check
// coinbase too and add it to block submission
