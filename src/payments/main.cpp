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
    std::vector<std::string> addresses;
    if (!redis_manager->GetAddresses(addresses)) return false;
    if (!redis_manager->LoadUnpaidRewards(payees, addresses)) return false;

    // filter only payees that reached their threshold
    std::erase_if(payees, [](const auto& p)
                  { return p.second.amount >= p.second.settings.threshold; });

    return true;
}

static constexpr std::string_view logger_field = "PaymentManager";
const Logger<logger_field> logger;

int UpdateTimeout(redisContext* redisCo, time_t payment_time, time_t curtime)
{
    // remove the error so we can continue using the reader.
    redisCo->err = 0;

    if (timeval timeout{.tv_sec = payment_time - curtime};
        redisSetTimeout(redisCo, timeout) == REDIS_ERR)
    {
        logger.Log<LogType::Error>("Failed to set redis timeout");
        return -1;
    }

    return 0;
}

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

    const std::string ip = "127.0.0.1";
    RedisPayment redis_manager(ip, &coinConfig);
    daemon_manager_t daemon_manager(coinConfig.payment_rpcs);
    time_t curtime = time(nullptr);

    time_t next_payment = curtime + coinConfig.payment_interval_seconds -
                          curtime % coinConfig.payment_interval_seconds;
    UpdateTimeout(redis_manager.rc, next_payment, curtime);

    redis_manager.SubscribeToMaturityChannel();
    // on each matured block: check if the updated
    // balances meets the payout threshold if yes
    // put miner on pending pending, stop waiting for replies if its time for
    // payout
    redisReply* reply;

    int res;
    while (res = redisGetReply(redis_manager.rc, (void**)&reply))
    {
        payees_info_t payees;

        // mature block update
        if (res == REDIS_ERR)
        {
            // REDIS_ERR_TIMEOUT is for windows...
            if (redis_manager.rc->err != REDIS_ERR_IO || errno == EAGAIN)
            {
                // other error;
                logger.Log<LogType::Error>("Redis error, code: {} -> {}",
                                           redis_manager.rc->err,
                                           redis_manager.rc->errstr);
                return -1;
            }

            // timeout reached, payment time!
            logger.Log<LogType::Info>("Payment time! {}", next_payment);
            next_payment += coinConfig.payment_interval_seconds;
            UpdateTimeout(redis_manager.rc, next_payment, curtime);

            // immediate payment time!
            GetPendingPayees(payees, &redis_manager);
            logger.Log<LogType::Info>("Payees pending payment: {}",
                                      payees.size());

            reward_map_t destinations;
            destinations.reserve(payees.size());

            // min fee
            // substract fees
            const int64_t fee = 10000000000; // 0.01

            if (payees.empty()) continue;

            const int64_t individual_fee = fee / payees.size();
            for (const auto& p : payees)
            {
                int64_t amount_txfeed = p.second.amount - individual_fee;
                destinations.emplace_back(p.first, amount_txfeed);
            }

            TransferResCn transfer_res;
            if (!daemon_manager.Transfer(transfer_res, destinations, fee,
                                         parser))
            {
            }
            continue;
        }
        // block matured
        else
        {
            UpdateTimeout(redis_manager.rc, next_payment, curtime);

            freeReplyObject(reply);
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