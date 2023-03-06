#ifndef CONFIG_PARSER_HPP_
#define CONFIG_PARSER_HPP_

#include "coin_config.hpp"
#include "logger.hpp"
#include "simdjson/simdjson.h"
static constexpr std::string_view config_field_str = "Config";

static constexpr auto CONFIG_PRINT_WIDTH = 40;

template <typename Doc>
void AssignJson(const char* name, std::string& obj, Doc& doc,
                const Logger& logger)
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
                const Logger& logger)
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
                const Logger& logger)
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
                     const Logger& logger)
{
    using namespace simdjson;
    ondemand::parser confParser;
    ondemand::document configDoc = confParser.iterate(json);

    AssignJson("symbol", cnfg.symbol, configDoc, logger);

    AssignJson("stratum_port", cnfg.stratum_port, configDoc, logger);
    AssignJson("control_port", cnfg.control_port, configDoc, logger);

    ondemand::object ob = configDoc["redis"].get_object();

    AssignJson("host", cnfg.redis.host, ob, logger);
    AssignJson("db_index", cnfg.redis.db_index, ob, logger);
    AssignJson("hashrate_ttl_seconds", cnfg.redis.hashrate_ttl_seconds, ob, logger);

    ob = configDoc["mysql"].get_object();
    AssignJson("host", cnfg.mysql.host, ob, logger);
    AssignJson("db_name", cnfg.mysql.db_name, ob, logger);
    AssignJson("user", cnfg.mysql.user, ob, logger);
    AssignJson("pass", cnfg.mysql.pass, ob, logger);

    ob = configDoc["stats"].get_object();

    AssignJson("hashrate_interval_seconds",
               cnfg.stats.hashrate_interval_seconds, ob, logger);

    AssignJson("effort_interval_seconds", cnfg.stats.effort_interval_seconds,
               ob, logger);
    AssignJson("average_hashrate_interval_seconds",
               cnfg.stats.average_hashrate_interval_seconds, ob, logger);
    AssignJson("mined_blocks_interval", cnfg.stats.mined_blocks_interval, ob,
               logger);

    AssignJson("socket_recv_timeout_seconds", cnfg.socket_recv_timeout_seconds,
               configDoc, logger);
    AssignJson("pow_fee", cnfg.pow_fee, configDoc, logger);
    AssignJson("pos_fee", cnfg.pos_fee, configDoc, logger);

    ob = configDoc["difficulty"].get_object();

    AssignJson("default_diff", cnfg.diff_config.default_diff, ob,
               logger);
    AssignJson("minimum_diff", cnfg.diff_config.minimum_diff, ob,
               logger);
    AssignJson("target_shares_rate", cnfg.diff_config.target_shares_rate, ob,
               logger);
    AssignJson("retarget_interval", cnfg.diff_config.retarget_interval, ob,
               logger);

    AssignJson("pool_addr", cnfg.pool_addr, configDoc, logger);
    AssignJson("block_poll_interval", cnfg.block_poll_interval, configDoc,
               logger);
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

        rpcs = configDoc["payment_rpcs"].get_array();
        for (auto rpc : rpcs)
        {
            RpcConfig rpcConf;
            std::string_view host_sv = rpc["host"].get_string();
            std::string_view auth_sv = rpc["auth"].get_string();
            rpcConf.host = std::string(host_sv);
            rpcConf.auth = std::string(auth_sv);
            cnfg.payment_rpcs.push_back(rpcConf);
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