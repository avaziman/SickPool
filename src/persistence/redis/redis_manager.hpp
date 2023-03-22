#ifndef REDIS_MANAGER_HPP_
#define REDIS_MANAGER_HPP_
#include <byteswap.h>
#include <fmt/format.h>
#include <sw/redis++/redis++.h>
#include <sw/redis++/pipeline.h>
#include <sw/redis++/connection_pool.h>
#include <sw/redis++/transaction.h>

#include <chrono>
#include <ctime>
#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>

#include "benchmark.hpp"
#include "coin_config.hpp"
#include "key_names.hpp"
#include "logger.hpp"
#include "redis_interop.hpp"
#include "round_share.hpp"
#include "shares/share.hpp"
#include "static_config/static_config.hpp"
#include "stats/stats.hpp"
#include "utils.hpp"

#define STRR(s) #s
#define xSTRR(s) STRR(s)

struct TsAggregation
{
    std::string type;
    int64_t time_bucket_ms;
};

// TODO: update pending payout on payment settings change

enum class DuplicatePolicy
{
    BLOCK,
    SUM
};

struct TimeSeries
{
    uint64_t retention_ms;
    DuplicatePolicy duplicate_policy;
    using labelsT = std::vector<std::pair<std::string_view, std::string_view>>;
    labelsT labels;

    explicit TimeSeries(uint64_t retention, const labelsT& labels,
                        DuplicatePolicy dp = DuplicatePolicy::SUM)
        : retention_ms(retention), duplicate_policy(dp), labels(labels)
    {
    }

    std::vector<std::string> GetCreateParams(std::string_view key) const
    {
        std::string dup_policy = "BLOCK";
        if (duplicate_policy == DuplicatePolicy::SUM)
        {
            dup_policy = "SUM";
        }

        std::vector<std::string> args;
        args.reserve(7 + labels.size() * 2);
        args.emplace_back("TS.CREATE");
        args.emplace_back(key);
        args.emplace_back("RETENTION");
        args.emplace_back(std::to_string(retention_ms));
        args.emplace_back("DUPLICATE_POLICY");
        args.emplace_back(dup_policy);
        args.emplace_back("LABELS");

        for (const auto& [label_key, val] : labels)
        {
            args.emplace_back(label_key);
            args.emplace_back(val);
        }
        return args;
    }
};

struct AddressTimeSeries : public TimeSeries
{
    explicit AddressTimeSeries(uint64_t retention, std::string_view address,
                               std::optional<std::string_view> alias,
                               const labelsT& labels = {},
                               DuplicatePolicy dp = DuplicatePolicy::SUM)
        : TimeSeries(retention, labels, dp)
    {
        this->labels.emplace_back("type", EnumName<Prefix::MINER>());
        this->labels.emplace_back("address", address);
        if (alias)
        {
            this->labels.emplace_back("alias", *alias);
        }
    }
};

struct WorkerTimeSeries : public AddressTimeSeries
{
    explicit WorkerTimeSeries(uint64_t retention, std::string_view address,
                              std::optional<std::string_view> alias,
                              std::string_view worker_name,
                              const labelsT& labels = {},
                              DuplicatePolicy dp = DuplicatePolicy::SUM)
        : AddressTimeSeries(retention, address, alias, labels, dp)
    {
        // change type from miner to worker
        this->labels[0].second = EnumName<Prefix::WORKER>();
        this->labels.emplace_back("worker_name", worker_name);
    }
};

class RedisTransaction;
class RedisManager
{
    friend class RedisTransaction;

   public:
    explicit RedisManager(const CoinConfig& cc);
    explicit RedisManager(const RedisManager& rm)
        : conf(rm.conf), key_names(rm.key_names)
    {
    }
    void Init();

   protected:
    static constexpr int ROUND_SHARES_LIMIT = 100;
    const CoinConfig* conf;
    const KeyNames key_names;

    static std::mutex rc_mutex;
    static constexpr std::string_view logger_field = "Redis";
    static const Logger logger;
    static std::unique_ptr<sw::redis::Redis> redis;

    void AppendTsCreate(sw::redis::Pipeline& pipe, std::string_view key,
                        const TimeSeries& ts);
    void AppendTsAdd(sw::redis::Pipeline& pipe, std::string_view key_name,
                     int64_t time, double value);
};

#endif
