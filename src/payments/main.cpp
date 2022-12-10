#include <errno.h>
#include <simdjson/simdjson.h>

#include "config_parser.hpp"
#include "daemon_manager_t.hpp"
#include "fmt/format.h"
#include "redis_payment.hpp"

using enum Prefix;

bool GetPendingPayees(payees_info_t& payees, RedisManager* redis_manager)
{
    // REDIS_OK
    std::vector<MinerIdHex> active_ids;

    if (!redis_manager->GetActiveIds(active_ids)) return false;
    if (!redis_manager->LoadUnpaidRewards(payees, active_ids)) return false;

    // filter only payees that reached their threshold
    std::erase_if(payees, [](const auto& p)
                  { return p.second.amount >= p.second.settings.threshold; });

    return true;
}

static constexpr std::string_view logger_field = "PaymentManager";
const Logger<logger_field> logger;

// on each matured pool block: check if the updated
// balances meets the payout threshold if yes
// put miner on pending, stop waiting for replies if its time for
// payout

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        logger.Log<LogType::Critical>("Not enough parameters.");
        return -1;
    }

    simdjson::ondemand::parser parser;
    Logger<config_field_str> conf_logger;
    CoinConfig coinConfig;
    simdjson::padded_string json = simdjson::padded_string::load(argv[1]);
    ParseCoinConfig(json, coinConfig, conf_logger);

    if (coinConfig.payment_interval_seconds <= 0)
    {
        logger.Log<LogType::Critical>("No payment interval.");
        return -1;
    }

    const PersistenceLayer rm(coinConfig);
    PersistenceBlock persistence_block(rm);
    daemon_manager_t daemon_manager(coinConfig.payment_rpcs);

    persistence_block.SubscribeToMaturityChannel();

    while (true)
    {
        time_t curtime = time(nullptr);
        time_t next_payment = curtime + coinConfig.payment_interval_seconds -
                              curtime % coinConfig.payment_interval_seconds;

        auto [res, rep] = persistence_block.GetOneReply(next_payment - curtime);

        payees_info_t payees;

        // payout time!
        if (res == REDIS_ERR)
        {
            // REDIS_ERR_TIMEOUT is for windows...
            if (RedisManager::GetError() != REDIS_ERR_IO || errno == EAGAIN)
            {
                logger.Log<LogType::Error>("Redis error, code: {}",
                                           RedisManager::GetError());
                return -1;
            }

            // timeout reached, payment time!
            logger.Log<LogType::Info>("Payment time! {}", next_payment);
            next_payment += coinConfig.payment_interval_seconds;

            // immediate payment time!
            GetPendingPayees(payees, &persistence_block);
            logger.Log<LogType::Info>("Payees pending payment: {}",
                                      payees.size());

            reward_map_t destinations;
            destinations.reserve(payees.size());

            // min fee
            // substract fees
            const int64_t fee = 10000000000;  // 0.01

            if (payees.empty()) continue;

            const int64_t individual_fee = fee / payees.size();
            for (const auto& [addr, info] : payees)
            {
                int64_t amount_txfeed = info.amount - individual_fee;
                destinations.emplace_back(addr, amount_txfeed);
            }

            TransferResCn transfer_res;
            if (!daemon_manager.Transfer(transfer_res, destinations, fee,
                                         parser))
            {
                logger.Log<LogType::Info>("Failed to transfer funds!");
            }
        }
        else {
        // pool block matured

        }
        // GetPendingPayees(payees, &redis_manager);
        // redis_manager.AddPendingPayees(payees);
        // for ( : payees)
        // {
        //     if (payee_info.amount < payee_info.settings.threshold)
        //     continue;

        // }

        // free consumed message
    }

    return 0;
}