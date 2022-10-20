#ifndef CONFIG_PARSER_HPP_
#define CONFIG_PARSER_HPP_

#include "coin_config.hpp"
#include "logger.hpp"
#include "simdjson/simdjson.h"

#define CONFIG_PRINT_WIDTH 40
template <typename Doc>
void AssignJson(const char* name, std::string& obj, Doc& doc,
                Logger<LogField::Config>& logger)
{
    try
    {
        std::string_view sv = doc[name].get_string();

        obj = std::string(sv);
    }
    catch (...)
    {
        throw std::runtime_error(fmt::format(
            "Invalid or no \"{}\" (expected string) variable in config file",
            name));
    }
    logger.Log<LogType::Info>("{:<{}}: {}", name, CONFIG_PRINT_WIDTH, obj);
}

template <typename Doc>
void AssignJson(const char* name, double& obj, Doc& doc,
                Logger<LogField::Config>& logger)
{
    try
    {
        obj = doc[name].get_double();
    }
    catch (...)
    {
        throw std::runtime_error(fmt::format(
            "Invalid or no \"{}\" (expected double) variable in config file",
            name));
    }
    logger.Log<LogType::Info>("{:<{}}: {}", name, CONFIG_PRINT_WIDTH, obj);
}

template <typename T, typename Doc>
void AssignJson(const char* name, T& obj, Doc& doc,
                Logger<LogField::Config>& logger)
{
    try
    {
        obj = static_cast<T>(doc[name].get_int64());
    }
    catch (...)
    {
        throw std::runtime_error(fmt::format(
            "Invalid or no \"{}\" (expected integer) variable in config file",
            name));
    }
    logger.Log<LogType::Info>("{:<{}}: {}", name, CONFIG_PRINT_WIDTH, obj);
}

void ParseCoinConfig(const simdjson::padded_string& json, CoinConfig& cnfg,
                     Logger<LogField::Config>& logger)
{
    using namespace simdjson;
    ondemand::parser confParser;
    ondemand::document configDoc = confParser.iterate(json);

    AssignJson("stratum_port", cnfg.stratum_port, configDoc, logger);
    AssignJson("control_port", cnfg.control_port, configDoc, logger);

    ondemand::object ob = configDoc["redis"].get_object();

    AssignJson("redis_port", cnfg.redis.redis_port, ob, logger);
    AssignJson("hashrate_ttl", cnfg.redis.hashrate_ttl_seconds, ob, logger);

    ob = configDoc["stats"].get_object();

    AssignJson("hashrate_interval_seconds",
               cnfg.stats.hashrate_interval_seconds, ob, logger);

    AssignJson("effort_interval_seconds", cnfg.stats.effort_interval_seconds,
               ob, logger);
    AssignJson("average_hashrate_interval_seconds",
               cnfg.stats.average_hashrate_interval_seconds, ob, logger);
    AssignJson("mined_blocks_interval", cnfg.stats.mined_blocks_interval, ob,
               logger);
    AssignJson("diff_adjust_seconds", cnfg.stats.diff_adjust_seconds, ob,
               logger);

    AssignJson("socket_recv_timeout_seconds", cnfg.socket_recv_timeout_seconds,
               configDoc, logger);
    AssignJson("pow_fee", cnfg.pow_fee, configDoc, logger);
    AssignJson("pos_fee", cnfg.pos_fee, configDoc, logger);
    AssignJson("default_difficulty", cnfg.default_difficulty, configDoc,
               logger);
    AssignJson("minimum_difficulty", cnfg.minimum_difficulty, configDoc,
               logger);
    AssignJson("target_shares_rate", cnfg.target_shares_rate, configDoc,
               logger);
    AssignJson("pool_addr", cnfg.pool_addr, configDoc, logger);
    AssignJson("payment_interval_seconds", cnfg.payment_interval_seconds,
               configDoc, logger);
    AssignJson("min_payout_threshold", cnfg.min_payout_threshold, configDoc,
               logger);

    try
    {
        auto rpcs = configDoc["rpcs"].get_array();
        for (auto rpc : rpcs)
        {
            RpcConfig rpcConf;
            std::string_view host_sv = rpc["host"].get_string();
            std::string_view auth_sv = rpc["auth"].get_string();
            rpcConf.host = std::string(host_sv);
            rpcConf.auth = std::string(auth_sv);
            cnfg.rpcs.push_back(rpcConf);
        }
    }
    catch (...)
    {
        throw std::runtime_error(
            std::string("Invalid or no \"") + "rpcs" +
            "\"; ([string host, string auth]) variable in config file");
    }
}

#endif